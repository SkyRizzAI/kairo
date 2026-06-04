#pragma once
#include "kairo/types.h"
#include "kairo/event/async_event_poster.h"
#include "kairo/services/input_service.h"
#include "kairo/services/audio_service.h"
#include "kairo/services/camera_service.h"
#include "kairo/nema/task_runner.h"
#include <memory>

namespace kairo {

struct IPlatform;
struct IBoard;
struct IClock;
class Logger;
struct ILogSink;
class EventBus;
class ServiceContainer;
class ServiceManager;
class HardwareRegistry;
class CapabilityRegistry;
struct SystemInfo;
class PluginManager;
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
    IClock&             clock();
    Logger&             log();
    EventBus&           events();
    ServiceContainer&   container();
    HardwareRegistry&   hardware();
    CapabilityRegistry& capabilities();
    const SystemInfo&   info() const;
    AsyncEventPoster&   asyncPoster();  // thread-safe cross-task event queue
    InputService&       input();        // single input funnel (any thread → main)
    nema::TaskRunner&   tasks();        // offload blocking work off the UI thread
    PluginManager&      plugins();
    ViewDispatcher&     view();
    Canvas&             canvas();   // only valid if "display" capability present
    DisplayPowerManager& dpm();     // sleep/lock state machine (only after start())
    IConfigStore&        config();  // persistent key-value store

    // GUI render FPS (actual display flushes/sec). Apps: ctx.runtime().fps().
    uint16_t fps()         const;
    bool     showFps()     const;
    void     setShowFps(bool on);
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
    std::unique_ptr<SystemInfo>        systemInfo_;
    std::unique_ptr<PluginManager>     pluginManager_;
    std::unique_ptr<ViewDispatcher>    viewDispatcher_;
    std::unique_ptr<Canvas>            canvas_;
    std::unique_ptr<GuiService>        gui_;           // UI thread (owns render)
    AsyncEventPoster                   asyncPoster_;   // value member — always alive
    InputService                       inputService_;  // value member — always alive
    nema::TaskRunner                   taskRunner_;    // value member — always alive
    AudioService                       audioService_;  // value member — always alive
    CameraService                      cameraService_; // value member — always alive
};

} // namespace kairo
