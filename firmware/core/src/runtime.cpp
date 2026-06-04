#include "kairo/runtime.h"
#include "kairo/platform.h"
#include "kairo/board.h"
#include "kairo/clock.h"
#include "kairo/log/logger.h"
#include "kairo/log/console_sink.h"
#include "kairo/log/memory_sink.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"
#include "kairo/service/service_container.h"
#include "kairo/service/service_manager.h"
#include "kairo/system/system_info.h"
#include "kairo/system/hardware_registry.h"
#include "kairo/system/capability_registry.h"
#include "kairo/plugin/plugin_manager.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/ui/screen.h"
#include "kairo/ui/canvas.h"
#include "kairo/event/async_event_poster.h"
#include "kairo/services/gui_service.h"
#include "kairo/services/display_power_manager.h"
#include "kairo/config/config_store.h"
#include "kairo/hal/display.h"
#include <cassert>

#ifndef KAIRO_BUILD_VERSION
  #define KAIRO_BUILD_VERSION "dev"
#endif
#ifndef KAIRO_FW_VERSION
  #define KAIRO_FW_VERSION "dev"
#endif

namespace kairo {

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
    systemInfo_   = std::make_unique<SystemInfo>();
    systemInfo_->buildVersion    = KAIRO_BUILD_VERSION;
    systemInfo_->firmwareVersion = KAIRO_FW_VERSION;
    systemInfo_->platformName    = platform_->name();
    systemInfo_->boardName       = board_->name();

    pluginManager_  = std::make_unique<PluginManager>(*this);
    viewDispatcher_ = std::make_unique<ViewDispatcher>();

    logger_->info("Runtime", "Core ready",
        {{"platform", platform_->name()}, {"board", board_->name()}});

    phase_ = BootPhase::CoreReady;
}

void Runtime::registerServices() {
    assert(phase_ == BootPhase::CoreReady);
    logger_->info("Runtime", "Registering services");

    platform_->registerDrivers(*this);
    board_->describeHardware(*this);

    // Build service manager now that container is populated
    serviceManager_ = std::make_unique<ServiceManager>(*container_, *logger_, *eventBus_, clock());

    // Create Canvas backed by the registered display driver (if any).
    // Logical scale: prefer config "display/scale", else the driver's dpi() hint,
    // else 1. A scale>1 makes a high-res panel present a logical surface so all
    // UI layout stays resolution-independent.
    if (capabilities_->has("display")) {
        if (auto* disp = container_->resolve<IDisplayDriver>()) {
            float scale = 0.0f;
            if (auto* cfg = container_->resolve<IConfigStore>()) {
                int64_t stored = cfg->getIntOr("display", "scale", 0);
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
    gui_ = std::make_unique<GuiService>(*this);
    gui_->start();         // spawn GUI thread — owns render + input dispatch
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
    if (gui_) gui_->stop();   // stop UI thread first — it touches screens/plugins
    taskRunner_.stop();       // join worker before plugins/services tear down
    pluginManager_->unloadAll();
    serviceManager_->stopAll();
    logger_->info("Runtime", "Shutdown complete");
}

AsyncEventPoster& Runtime::asyncPoster() { return asyncPoster_; }
InputService&     Runtime::input()       { return inputService_; }
nema::TaskRunner& Runtime::tasks()       { return taskRunner_; }

void Runtime::step() {
    uint64_t now = clock().millis();

    // Drain cross-task events FIRST — before any tick sees them as "already processed".
    // Background tasks (WiFi, BLE, NTP, etc.) post here; this publishes to EventBus
    // safely from the main task. Any subscriber seeing these events will fire during
    // the same frame's tick below.
    asyncPoster_.flush(*eventBus_);

    serviceManager_->tickAll(now);
    pluginManager_->tickAll(now);

    platform_->idle();
}

void Runtime::requestShutdown() { shutdownRequested_ = true; }
bool Runtime::isShutdownRequested() const { return shutdownRequested_; }
void Runtime::requestRestart()  { exitCode_ = 75; shutdownRequested_ = true; }

IPlatform&          Runtime::platform()      { assert(platform_); return *platform_; }
IClock&             Runtime::clock()         { assert(platform_); return platform_->clock(); }
Logger&             Runtime::log()           { assert(logger_);   return *logger_; }
EventBus&           Runtime::events()        { assert(eventBus_); return *eventBus_; }
ServiceContainer&   Runtime::container()     { assert(container_); return *container_; }
HardwareRegistry&   Runtime::hardware()      { assert(hardware_);  return *hardware_; }
CapabilityRegistry& Runtime::capabilities()  { assert(capabilities_); return *capabilities_; }
const SystemInfo&   Runtime::info()    const { assert(systemInfo_);  return *systemInfo_; }
PluginManager&      Runtime::plugins()       { assert(pluginManager_); return *pluginManager_; }
ViewDispatcher&     Runtime::view()          { assert(viewDispatcher_); return *viewDispatcher_; }
Canvas&             Runtime::canvas()        { assert(canvas_); return *canvas_; }
DisplayPowerManager& Runtime::dpm()          { assert(gui_); return gui_->dpm(); }
IConfigStore&        Runtime::config()       { auto* c = container_->resolve<IConfigStore>(); assert(c); return *c; }
BootPhase           Runtime::phase()   const { return phase_; }
int                 Runtime::exitCode() const { return exitCode_; }

} // namespace kairo
