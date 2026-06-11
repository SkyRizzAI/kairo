#include <emscripten.h>
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/wasm/wasm_platform.h"
#include "kairo/sim/simulator_board.h"
#include "kairo/services/clock_service.h"
#include "kairo/app/app_registry.h"
#include "kairo/apps/clock_app.h"
#include "kairo/apps/counter_app.h"
#include "kairo/apps/stopwatch_app.h"
#include "kairo/apps/task_demo_app.h"
#include "kairo/apps/ticker_app.h"
#include "kairo/apps/ui_showcase_app.h"
#include "kairo/apps/js_app_store.h"
#include "kairo/screens/home_screen.h"
#include "kairo/ui/view_dispatcher.h"

namespace {
    kairo::WasmPlatform           platform;
    kairo::SimulatorBoard         board;
    kairo::Runtime                rt = kairo::Runtime::create();
}

static void loop() { rt.step(); }

int main() {
    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    // Background service shipped via the same registry as apps (AppType::
    // Service, hidden from the launcher). Installed before start() → boots
    // with the system.
    static kairo::ClockService clockSvc(rt.log(), rt.events());
    rt.apps().installService(clockSvc, "com.kairo.svc.clock");

    rt.start();

    // Install the built-in apps (shown in the launcher; selecting spawns the
    // app on its own thread).
    static kairo::ClockApp      clockApp;
    static kairo::CounterApp    counterApp;
    static kairo::StopwatchApp  stopwatchApp;
    static kairo::TaskDemoApp   taskDemoApp;
    static kairo::TickerApp     tickerApp;
    static kairo::UiShowcaseApp uiShowcaseApp;
    rt.apps().install(clockApp);
    rt.apps().install(counterApp);
    rt.apps().install(stopwatchApp);
    rt.apps().install(taskDemoApp);
    rt.apps().install(tickerApp);
    rt.apps().install(uiShowcaseApp);
    kairo::loadEmbeddedJsApps(rt);   // built-in JS (custom) apps (Plan 37)

    static kairo::HomeScreen homeScreen(rt);
    rt.view().push(homeScreen);

    rt.log().info("Boot", "wasm ready");

    emscripten_set_main_loop(loop, 0, 1);  // rAF-driven; simulate infinite loop
    return 0;
}
