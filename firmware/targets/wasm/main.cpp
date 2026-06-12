#include <emscripten.h>
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/wasm/wasm_platform.h"
#include "nema/sim/simulator_board.h"
#include "nema/services/clock_service.h"
#include "nema/app/app_registry.h"
#include "nema/apps/clock_app.h"
#include "nema/apps/counter_app.h"
#include "nema/apps/stopwatch_app.h"
#include "nema/apps/task_demo_app.h"
#include "nema/apps/ticker_app.h"
#include "nema/apps/ui_showcase_app.h"
#include "nema/apps/js_app_store.h"
#include "nema/screens/home_screen.h"
#include "nema/ui/view_dispatcher.h"

namespace {
    nema::WasmPlatform           platform;
    nema::SimulatorBoard         board;
    nema::Runtime                rt = nema::Runtime::create();
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
    static nema::ClockService clockSvc(rt.log(), rt.events());
    rt.apps().installService(clockSvc, "com.palanu.svc.clock");

    rt.start();

    // Install the built-in apps (shown in the launcher; selecting spawns the
    // app on its own thread).
    static nema::ClockApp      clockApp;
    static nema::CounterApp    counterApp;
    static nema::StopwatchApp  stopwatchApp;
    static nema::TaskDemoApp   taskDemoApp;
    static nema::TickerApp     tickerApp;
    static nema::UiShowcaseApp uiShowcaseApp;
    rt.apps().install(clockApp);
    rt.apps().install(counterApp);
    rt.apps().install(stopwatchApp);
    rt.apps().install(taskDemoApp);
    rt.apps().install(tickerApp);
    rt.apps().install(uiShowcaseApp);
    nema::loadEmbeddedJsApps(rt);   // built-in JS (custom) apps (Plan 37)

    static nema::HomeScreen homeScreen(rt);
    rt.view().push(homeScreen);

    rt.log().info("Boot", "wasm ready");

    emscripten_set_main_loop(loop, 0, 1);  // rAF-driven; simulate infinite loop
    return 0;
}
