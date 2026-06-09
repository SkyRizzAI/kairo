#include <emscripten.h>
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/wasm/wasm_platform.h"
#include "kairo/sim/simulator_board.h"
#include "kairo/service/service_container.h"
#include "kairo/services/clock_service.h"
#include "kairo/plugins/hello_plugin.h"
#include "kairo/plugins/clock_plugin.h"
#include "kairo/plugins/counter_plugin.h"
#include "kairo/plugins/stopwatch_plugin.h"
#include "kairo/plugins/task_demo_plugin.h"
#include "kairo/plugins/ticker_plugin.h"
#include "kairo/plugins/ui_showcase_plugin.h"
#include "kairo/plugins/js_app_store.h"
#include "kairo/plugin/plugin_manager.h"
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

    static kairo::ClockService clockSvc(rt.log(), rt.events());
    rt.container().registerService(&clockSvc);

    rt.start();

    static kairo::HelloPlugin      helloPlugin;
    static kairo::ClockPlugin      clockPlugin;
    static kairo::CounterPlugin    counterPlugin;
    static kairo::StopwatchPlugin  stopwatchPlugin;
    static kairo::TaskDemoPlugin   taskDemoPlugin;
    static kairo::TickerPlugin     tickerPlugin;
    static kairo::UiShowcasePlugin uiShowcasePlugin;
    rt.plugins().load(helloPlugin);
    rt.plugins().load(clockPlugin);
    rt.plugins().load(counterPlugin);
    rt.plugins().load(stopwatchPlugin);
    rt.plugins().load(taskDemoPlugin);
    rt.plugins().load(tickerPlugin);
    rt.plugins().load(uiShowcasePlugin);
    kairo::loadEmbeddedJsApps(rt);   // built-in JS apps (Plan 37)

    static kairo::HomeScreen homeScreen(rt);
    rt.view().push(homeScreen);

    rt.log().info("Boot", "wasm ready");

    emscripten_set_main_loop(loop, 0, 1);  // rAF-driven; simulate infinite loop
    return 0;
}
