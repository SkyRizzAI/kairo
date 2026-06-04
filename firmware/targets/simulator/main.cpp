#include "kairo/runtime.h"
#include "kairo/sim/simulator_platform.h"
#include "kairo/sim/simulator_board.h"
#include "kairo/service/service_container.h"
#include "kairo/services/clock_service.h"
#include "kairo/plugins/hello_plugin.h"
#include "kairo/plugins/clock_plugin.h"
#include "kairo/plugins/counter_plugin.h"
#include "kairo/plugins/stopwatch_plugin.h"
#include "kairo/plugins/task_demo_plugin.h"
#include "kairo/plugins/ticker_plugin.h"
#include "kairo/plugin/plugin_manager.h"
#include "kairo/screens/home_screen.h"
#include "kairo/ui/view_dispatcher.h"

int main() {
    kairo::SimulatorPlatform platform;
    kairo::SimulatorBoard    board;

    kairo::Runtime rt = kairo::Runtime::create();
    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    // Core background services
    kairo::ClockService clockSvc(rt.log(), rt.events());
    rt.container().registerService(&clockSvc);

    rt.start();  // → startAll() + SystemReady + snapshots

    // Load plugins
    kairo::HelloPlugin     helloPlugin;
    kairo::ClockPlugin     clockPlugin;
    kairo::CounterPlugin   counterPlugin;
    kairo::StopwatchPlugin stopwatchPlugin;
    kairo::TaskDemoPlugin  taskDemoPlugin;
    kairo::TickerPlugin    tickerPlugin;
    rt.plugins().load(helloPlugin);
    rt.plugins().load(clockPlugin);
    rt.plugins().load(counterPlugin);
    rt.plugins().load(stopwatchPlugin);
    rt.plugins().load(taskDemoPlugin);
    rt.plugins().load(tickerPlugin);

    // Push home screen
    kairo::HomeScreen homeScreen(rt);
    rt.view().push(homeScreen);

    rt.run();

    return rt.exitCode();
}
