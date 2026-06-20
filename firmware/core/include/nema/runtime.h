#pragma once
#include "nema/types.h"
#include "nema/event/async_event_poster.h"
#include "nema/services/input_service.h"
#include "nema/services/audio_service.h"
#include "nema/services/camera_service.h"
#include "nema/task_runner.h"
#include "nema/proc/process_manager.h"
#include "nema/services/display_power_manager.h"
#include <memory>
#include <vector>
#include <atomic>

namespace nema {

struct IPlatform;
struct IBoard;
struct IClock;
class Logger;
struct ILogSink;
struct IFileSystem;
class EventBus;
class ServiceContainer;
class ServiceManager;
struct IService;
class HardwareRegistry;
class CapabilityRegistry;
class CliSessionManager;
class CliService;
struct SystemInfo;
class AppRegistry;
class AppHostManager;
class ProcessManager;
class ViewDispatcher;
class Canvas;
class IConfigStore;
struct IDisplayServer;
class DummyBatteryDriver;
class NtpService;

class Runtime {
public:
    static Runtime create();
    ~Runtime();

    void loadPlatform(IPlatform& p);
    void loadBoard(IBoard& b);
    void initCore();
    void registerServices();
    void start();
    void run();      // blocking loop: while(!shutdown) step();  — host/simulator
    void step();     // one iteration (tick + render) — Arduino loop() calls this
    void requestShutdown();
    void requestRestart();
    void requestBootloader();
    bool isShutdownRequested() const;

    IPlatform&          platform();
    IBoard&             board();
    IClock&             clock();
    Logger&             log();
    EventBus&           events();
    ServiceContainer&   container();
    HardwareRegistry&   hardware();
    CapabilityRegistry& capabilities();
    CliSessionManager&  cliSessions();   // live CLI shell sessions (Plan 45)
    ServiceState        serviceState(IService* svc) const;  // for `ps` (Plan 46)
    const SystemInfo&   info() const;
    AsyncEventPoster&   asyncPoster();  // thread-safe cross-task event queue
    InputService&       input();        // single input funnel (any thread → main)
    nema::TaskRunner&   tasks();        // offload blocking work off the UI thread
    AppRegistry&        apps();       // installed-app table: install/list/launch
    AppHostManager&     appHost();    // app loader: launch IApp + pause/resume (Plan 22)
    ProcessManager&     processes();  // live process tracker (Plan 54)

    // Hand a background service to the Nema lifecycle. Before start(): it boots
    // with the system (startAll). While Running: it starts immediately. Used by
    // the AppRegistry for app-installed services; targets go through
    // apps().installService() instead of calling this directly.
    void adoptService(IService* svc);
    void dropService (IService* svc);   // stop (if running) + untrack
    ViewDispatcher&     view();
    Canvas&             canvas();   // only valid if "display" capability present
    DisplayPowerManager& dpm();     // sleep/lock state machine (only after start())
    IConfigStore&        config();  // persistent key-value store

    // GUI render FPS (actual display flushes/sec). Apps: ctx.runtime().fps().
    uint16_t fps()         const;
    bool     showFps()     const;
    void     setShowFps(bool on);

    // Display server control (Plan 43) — forwarded to GuiService. Used by the
    // CLI `display` command to switch the rendering backend at runtime.
    bool                     switchDisplayServer(const char* name);
    const char*              displayServerName() const;
    std::vector<const char*> displayServerList() const;
    IDisplayServer*          displayServer() const;      // Plan 50/51: active server
    IDisplayServer*          findDisplayServer(const char* name) const;  // Plan 51: by name

    // Plan 80 — the display servers are constructed and owned by the target's
    // main (in the aether lib / a server module), then registered here so core
    // (AppRegistry caps-checks, CLI `display`) can address them via the neutral
    // IDisplayServer contract without depending on any server type. `boot=true`
    // makes this server the initially-active one. applyPendingServer() is called
    // on the GUI thread to fulfil a switchDisplayServer() requested elsewhere.
    void registerDisplayServer(IDisplayServer* s, bool boot = false);
    bool applyPendingServer();   // GUI thread: pending→active; true if swapped
    AudioService&        audio();
    CameraService&       camera();

    // Wire up the platform's CliService so FbconServer can execute commands
    // from the on-device console. Called by each platform after CLI setup.
    void         setCli(CliService& c)  { cli_ = &c; }
    CliService*  cliService()           { return cli_; }

    // Platform filesystem (Plan 59). Platforms call setFs(&vfs_) during
    // registerDrivers(). The PAPP1 installer uses fs() to persist bundles to
    // /flash/apps/<id>/. May be nullptr on platforms with no flash filesystem.
    void          setFs(IFileSystem* fs) { fs_ = fs; }
    IFileSystem*  fs()                   { return fs_; }

    // Enumerate the in-memory log ring buffer oldest→newest (Plan 60 LogsScreen).
    // Callback-based so callers don't depend on the sink's container type, and
    // it avoids assuming contiguous storage (the sink is a std::deque).
    void logForEach(void (*fn)(void* ctx, const struct LogEntry&), void* ctx) const;
    size_t logCount() const;

    BootPhase phase()    const;
    int       exitCode() const;

private:
    Runtime() = default;

    IPlatform* platform_ = nullptr;
    IBoard*    board_    = nullptr;
    BootPhase  phase_    = BootPhase::None;
    bool       shutdownRequested_ = false;
    int        exitCode_          = 0;

    std::unique_ptr<ILogSink>          consoleSink_;
    std::unique_ptr<ILogSink>          memorySink_;
    std::unique_ptr<Logger>            logger_;
    std::unique_ptr<EventBus>          eventBus_;
    std::unique_ptr<ServiceContainer>  container_;
    std::unique_ptr<ServiceManager>    serviceManager_;
    std::unique_ptr<HardwareRegistry>  hardware_;
    std::unique_ptr<CapabilityRegistry> capabilities_;
    std::unique_ptr<CliSessionManager>  cliSessions_;
    std::unique_ptr<SystemInfo>        systemInfo_;
    std::unique_ptr<AppRegistry>       appRegistry_;
    std::unique_ptr<AppHostManager>    appHosts_;
    ProcessManager                     processes_;      // value member — always alive
    std::unique_ptr<ViewDispatcher>    viewDispatcher_;
    std::unique_ptr<Canvas>            canvas_;
    // Plan 80: the UI render loop (GuiService) is owned by the target main and
    // lives in the display-server lib, not here. Core keeps only the neutral
    // sleep/lock state machine + the registry of IDisplayServer backends.
    DisplayPowerManager                dpm_;           // sleep/lock (GuiService inits it)
    std::vector<IDisplayServer*>       displayServers_;// registered backends (owned by main)
    IDisplayServer*                    activeServer_ = nullptr;
    std::atomic<IDisplayServer*>       pendingServer_{nullptr};
    AsyncEventPoster                   asyncPoster_;   // value member — always alive
    InputService                       inputService_;  // value member — always alive
    nema::TaskRunner                   taskRunner_;    // value member — always alive
    AudioService                       audioService_;  // value member — always alive
    CliService*                        cli_           = nullptr;
    IFileSystem*                       fs_            = nullptr;
    CameraService                      cameraService_; // value member — always alive
    std::unique_ptr<DummyBatteryDriver> dummyBattery_;
    std::unique_ptr<NtpService>        ntp_;
};

} // namespace nema
