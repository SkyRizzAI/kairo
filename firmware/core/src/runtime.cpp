#include "nema/system/capabilities.h"
#include "nema/runtime.h"
#include "nema/platform.h"
#include "nema/board.h"
#include "nema/clock.h"
#include "nema/log/logger.h"
#include "nema/log/console_sink.h"
#include "nema/log/memory_sink.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/service/service_container.h"
#include "nema/service/service_manager.h"
#include "nema/services/cli_service.h"
#include "nema/system/system_info.h"
#include "nema/system/hardware_registry.h"
#include "nema/system/capability_registry.h"
#include "nema/app/app_registry.h"
#include "nema/app/app_host_manager.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/screen.h"
#include "nema/ui/canvas.h"
#include "nema/event/async_event_poster.h"
#include "nema/ui/display_server.h"
#include "nema/services/display_power_manager.h"
#include "nema/config/config_store.h"
#include "nema/hal/display.h"
#include "nema/services/dummy_battery_driver.h"
#include "nema/services/ntp_service.h"
#include "nema/version.h"
#include "nema/ui/ui_profile.h"
#include <cassert>

namespace nema {

Runtime::~Runtime() = default;

Runtime Runtime::create() { return Runtime{}; }

void Runtime::loadPlatform(IPlatform& p) {
    assert(p.name());
    platform_ = &p;
    phase_ = BootPhase::PlatformLoaded;
}

void Runtime::loadBoard(IBoard& b) {
    assert(platform_);
    board_ = &b;
    phase_ = BootPhase::BoardLoaded;
}

void Runtime::initCore() {
    assert(platform_ && board_);

    memorySink_ = std::make_unique<MemorySink>(1024);
    logger_     = std::make_unique<Logger>(platform_->clock());
    if (platform_->outputMode() == IPlatform::OutputMode::Human) {
        consoleSink_ = std::make_unique<ConsoleSink>();
        logger_->addSink(*consoleSink_);
    }
    logger_->addSink(*memorySink_);
    logger_->info("Logger", "Initialized");

    eventBus_     = std::make_unique<EventBus>();
    container_    = std::make_unique<ServiceContainer>();
    hardware_     = std::make_unique<HardwareRegistry>();
    capabilities_ = std::make_unique<CapabilityRegistry>();
    capabilities_->setBus(eventBus_.get());   // liveness setState() → ResourceChanged
    cliSessions_  = std::make_unique<CliSessionManager>();
    systemInfo_   = std::make_unique<SystemInfo>();
    systemInfo_->buildVersion    = NEMA_BUILD_HASH;
    systemInfo_->firmwareVersion = NEMA_FULL_VERSION;
    systemInfo_->platformName    = platform_->name();
    systemInfo_->boardName       = board_->name();

    appRegistry_    = std::make_unique<AppRegistry>(*this);
    appHosts_       = std::make_unique<AppHostManager>(*this);
    viewDispatcher_ = std::make_unique<ViewDispatcher>();

    // Plan 97 — event-driven GUI: input posts wake the GUI loop immediately
    // instead of waiting out the frame poll. (AppHost::present wakes it too.)
    inputService_.setWaker(&guiWaker_);

    logger_->info("Runtime", "Core ready",
        {{"platform", platform_->name()}, {"board", board_->name()}});

    phase_ = BootPhase::CoreReady;
}

void Runtime::registerServices() {
    assert(phase_ == BootPhase::CoreReady);
    logger_->info("Runtime", "Registering services");

    platform_->registerDrivers(*this);
    board_->describeHardware(*this);
    // Let the platform decorate board-provided drivers (e.g. wrap the display
    // with the remote screen-tap) before the Canvas binds to them below.
    platform_->postRegister(*this);

    // Build service manager now that container is populated
    serviceManager_ = std::make_unique<ServiceManager>(*container_, *logger_, *eventBus_);

    // Create Canvas backed by the registered display driver (if any).
    // Logical scale: prefer config "display/scale", else the driver's dpi() hint,
    // else 1. A scale>1 makes a high-res panel present a logical surface so all
    // UI layout stays resolution-independent.
    if (capabilities_->has(caps::Display)) {
        if (auto* disp = container_->resolve<IDisplayDriver>()) {
            float scale = 0.0f;
            if (auto* cfg = container_->resolve<IConfigStore>()) {
                int64_t stored = cfg->getIntOr("aether", "scale", 0);
                if (stored > 10) {
                    scale = stored / 100.0f;  // new format: value*100 (e.g. 125 → 1.25)
                } else if (stored >= 1) {
                    scale = (float)stored;    // old integer format (1, 2, 3)
                }
            }
            if (scale < 1.0f) {
                uint16_t dpi = disp->dpi();
                scale = (dpi >= 200) ? 2.0f : 1.0f;
            }
            canvas_ = std::make_unique<Canvas>(*disp, scale);
        }
    }

    // Resource liveness bridge (Plan 42): net/bt are DYNAMIC — a built-in radio
    // is not "connected" until it associates. Seed them Absent, then mirror the
    // existing connect/disconnect events into liveness. These handlers run on the
    // MAIN thread (EventBus dispatch happens during asyncPoster_.flush() in
    // step()), so setState() is called safely — never from a driver's bg task.
    if (capabilities_->has(caps::NetWifi)) capabilities_->setState(caps::NetWifi, ResourceState::Absent);
    if (capabilities_->has(caps::BtBle))   capabilities_->setState(caps::BtBle,   ResourceState::Absent);
    eventBus_->subscribe(events::NetworkConnected,
        [this](const Event&) { capabilities_->setState(caps::NetWifi, ResourceState::Available); });
    eventBus_->subscribe(events::NetworkDisconnected,
        [this](const Event&) { capabilities_->setState(caps::NetWifi, ResourceState::Absent); });
    eventBus_->subscribe(events::BtConnected,
        [this](const Event&) { capabilities_->setState(caps::BtBle, ResourceState::Available); });
    eventBus_->subscribe(events::BtDisconnected,
        [this](const Event&) { capabilities_->setState(caps::BtBle, ResourceState::Absent); });

    // Plan 64 — crash recovery subscriptions.
    appHosts_->initCrashRecovery();

    // Plan 65 — dummy battery driver (on platforms with display).
    if (capabilities_->has(caps::Display)) {
        dummyBattery_ = std::make_unique<DummyBatteryDriver>();
        dummyBattery_->onRegister(*this);
        adoptService(dummyBattery_.get());
        container_->registerAs<IBatteryDriver>(dummyBattery_.get());   // reachable via rt.container()
        capabilities_->add(caps::Battery);
    }

    // LED effect engine — adopt so the non-blocking blink/notify loop ticks on
    // boards that declare an LED (registry may be empty → tick is a no-op).
    if (capabilities_->has(caps::Rgb) || capabilities_->has(caps::Led))
        adoptService(&ledService_);

    // Plan 62 — NTP time sync (on platforms with networking).
    if (capabilities_->has(caps::NetWifi)) {
        ntp_ = std::make_unique<NtpService>(*this);
        adoptService(ntp_.get());
    }

    std::string caps;
    for (const auto& c : capabilities_->list()) caps += c + " ";
    logger_->info("Runtime", "Capabilities: " + caps);

    eventBus_->publish({events::SystemBoot, {}});
    phase_ = BootPhase::ServicesRegistered;
    logger_->info("Runtime", "Services registered");
}

void Runtime::start() {
    assert(phase_ == BootPhase::ServicesRegistered);
    serviceManager_->startAll();
    taskRunner_.start();   // spawn background worker — UI thread never blocks
    // Plan 80: the UI render loop (GuiService) + display servers are constructed
    // and started by the target main (it links the display-server lib). Core no
    // longer owns the loop; it only exposes the DPM + the IDisplayServer registry.
    phase_ = BootPhase::Running;
    eventBus_->publish({events::SystemReady, {}});
    logger_->info("Runtime", "Started");
}

void Runtime::run() {
    assert(phase_ == BootPhase::Running);
    // Initial render
    if (viewDispatcher_ && !viewDispatcher_->empty()) {
        viewDispatcher_->requestRedraw();
    }
    while (!shutdownRequested_) {
        step();
    }
    logger_->info("Runtime", "Shutdown requested — stopping");
    // GuiService (owned by main) stops itself on teardown; core only stops its own.
    taskRunner_.stop();       // join worker before services tear down
    serviceManager_->stopAll();
    logger_->info("Runtime", "Shutdown complete");
}

AsyncEventPoster& Runtime::asyncPoster() { return asyncPoster_; }
InputService&     Runtime::input()       { return inputService_; }
Waker&            Runtime::guiWaker()    { return guiWaker_; }
nema::TaskRunner& Runtime::tasks()       { return taskRunner_; }
AudioService&     Runtime::audio()       { return audioService_; }
CameraService&    Runtime::camera()      { return cameraService_; }
LedService&       Runtime::led()         { return ledService_; }
SensorService&    Runtime::sensors()     { return sensorService_; }

void Runtime::step() {
    uint64_t now = clock().millis();

    // Drain cross-task events FIRST — before any tick sees them as "already processed".
    // Background tasks (WiFi, BLE, NTP, etc.) post here; this publishes to EventBus
    // safely from the main task. Any subscriber seeing these events will fire during
    // the same frame's tick below.
    asyncPoster_.flush(*eventBus_);

    serviceManager_->tickAll(now);
    // Apps run on their own threads (AppHostManager) — no cooperative per-frame
    // tick here; background work belongs in Services, which ticked above.

    platform_->idle();
}

void Runtime::requestShutdown() {
    shutdownRequested_ = true;                         // host/sim: run() loop exits
    if (platform_) platform_->power(IPlatform::PowerAction::Shutdown);  // hardware: sleep
}
bool Runtime::isShutdownRequested() const { return shutdownRequested_; }
void Runtime::requestRestart()  {
    exitCode_ = 75; shutdownRequested_ = true;         // host/sim: run() loop exits (75 = restart)
    if (platform_) platform_->power(IPlatform::PowerAction::Restart);  // hardware: reboot now
}
void Runtime::requestBootloader() {
    exitCode_ = 75; shutdownRequested_ = true;         // host/sim: run() loop exits (75 = restart)
    if (platform_) platform_->power(IPlatform::PowerAction::Bootloader);  // hardware: GPIO0 low + reboot
}

IPlatform&          Runtime::platform()      { assert(platform_); return *platform_; }
IBoard&             Runtime::board()         { assert(board_);    return *board_; }
IClock&             Runtime::clock()         { assert(platform_); return platform_->clock(); }
Logger&             Runtime::log()           { assert(logger_);   return *logger_; }
EventBus&           Runtime::events()        { assert(eventBus_); return *eventBus_; }
ServiceContainer&   Runtime::container()     { assert(container_); return *container_; }
HardwareRegistry&   Runtime::hardware()      { assert(hardware_);  return *hardware_; }
CapabilityRegistry& Runtime::capabilities()  { assert(capabilities_); return *capabilities_; }
CliSessionManager&  Runtime::cliSessions()   { assert(cliSessions_); return *cliSessions_; }
ServiceState Runtime::serviceState(IService* svc) const {
    return serviceManager_ ? serviceManager_->stateOf(svc) : ServiceState::Created;
}
const SystemInfo&   Runtime::info()    const { assert(systemInfo_);  return *systemInfo_; }
AppRegistry&        Runtime::apps()          { assert(appRegistry_); return *appRegistry_; }
AppHostManager&     Runtime::appHost()       { assert(appHosts_); return *appHosts_; }
ProcessManager&     Runtime::processes()     { return processes_; }

void Runtime::adoptService(IService* svc) {
    assert(svc && container_ && serviceManager_);
    container_->addService(svc);
    // Before start(): startAll() will pick it up at boot. Already Running:
    // bring it up now so runtime-installed services actually run.
    if (phase_ == BootPhase::Running) serviceManager_->startOne(svc);
}

void Runtime::dropService(IService* svc) {
    assert(svc && container_ && serviceManager_);
    serviceManager_->stopOne(svc);
    container_->removeService(svc);
}
ViewDispatcher&     Runtime::view()          { assert(viewDispatcher_); return *viewDispatcher_; }
Canvas&             Runtime::canvas()        { assert(canvas_); return *canvas_; }
DisplayPowerManager& Runtime::dpm()          { return dpm_; }
IConfigStore&        Runtime::config()       { auto* c = container_->resolve<IConfigStore>(); assert(c); return *c; }
// FPS is forwarded to the active server (the IDisplayServer contract carries it;
// non-graphical servers default to 0/false/no-op).
uint16_t            Runtime::fps()     const { return activeServer_ ? activeServer_->fps() : 0; }
bool                Runtime::showFps() const { return activeServer_ && activeServer_->showFps(); }
void                Runtime::setShowFps(bool on) { if (activeServer_) activeServer_->setShowFps(on); }

void Runtime::registerDisplayServer(IDisplayServer* s, bool boot) {
    if (!s) return;
    displayServers_.push_back(s);
    if (boot || !activeServer_) activeServer_ = s;   // first/boot becomes active
}
bool Runtime::applyPendingServer() {
    if (auto* next = pendingServer_.exchange(nullptr)) { activeServer_ = next; return true; }
    return false;
}
bool Runtime::switchDisplayServer(const char* name) {
    IDisplayServer* t = findDisplayServer(name);
    if (!t) return false;
    pendingServer_.store(t);   // GUI thread applies it via applyPendingServer()
    return true;
}
const char* Runtime::displayServerName() const { return activeServer_ ? activeServer_->name() : "none"; }
std::vector<const char*> Runtime::displayServerList() const {
    std::vector<const char*> v;
    for (IDisplayServer* s : displayServers_) v.push_back(s->name());
    return v;
}
IDisplayServer* Runtime::displayServer()     const { return activeServer_; }
IDisplayServer* Runtime::findDisplayServer(const char* name) const {
    if (!name) return nullptr;
    for (IDisplayServer* s : displayServers_)
        if (std::string(s->name()) == name) return s;
    return nullptr;
}
BootPhase           Runtime::phase()   const { return phase_; }
int                 Runtime::exitCode() const { return exitCode_; }

void Runtime::logForEach(void (*fn)(void* ctx, const LogEntry&), void* ctx) const {
    auto* ms = dynamic_cast<MemorySink*>(memorySink_.get());
    if (!ms || !fn) return;
    for (const LogEntry& e : ms->entries()) fn(ctx, e);
}

size_t Runtime::logCount() const {
    auto* ms = dynamic_cast<MemorySink*>(memorySink_.get());
    return ms ? ms->entries().size() : 0;
}

void Runtime::logClear() {
    if (auto* ms = dynamic_cast<MemorySink*>(memorySink_.get())) ms->clear();
}

aether::ui::UiProfile Runtime::uiProfile() const {
    return aether::ui::UiProfile::fromCapabilities(*capabilities_);
}

} // namespace nema
