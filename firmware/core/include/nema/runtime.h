#pragma once
#include "nema/types.h"
#include "nema/event/async_event_poster.h"
#include "nema/services/input_service.h"
#include "nema/services/audio_service.h"
#include "nema/services/camera_service.h"
#include "nema/task_runner.h"
#include <memory>
#include <vector>

namespace nema {

struct IPlatform;
struct IBoard;
struct IClock;
class Logger;
struct ILogSink;
class EventBus;
class ServiceContainer;
class ServiceManager;
struct IService;
class HardwareRegistry;
class CapabilityRegistry;
class CliSessionManager;
struct SystemInfo;
class AppRegistry;
class AppHostManager;
class ViewDispatcher;
class Canvas;
class GuiService;
class DisplayPowerManager;
class IConfigStore;

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
    AudioService&        audio();
    CameraService&       camera();

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
    std::unique_ptr<ViewDispatcher>    viewDispatcher_;
    std::unique_ptr<Canvas>            canvas_;
    std::unique_ptr<GuiService>        gui_;           // UI thread (owns render)
    AsyncEventPoster                   asyncPoster_;   // value member — always alive
    InputService                       inputService_;  // value member — always alive
    nema::TaskRunner                   taskRunner_;    // value member — always alive
    AudioService                       audioService_;  // value member — always alive
    CameraService                      cameraService_; // value member — always alive
};

} // namespace nema
