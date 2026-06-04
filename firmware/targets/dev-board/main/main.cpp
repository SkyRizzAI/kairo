// Kairo Dev Board — ESP32-S3 + e-ink entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
// Same boot flow as the simulator target; only Platform + Board differ.
#include <Arduino.h>

#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/esp32/esp32_platform.h"
#include "kairo/devboard/dev_board.h"
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

// Runtime objects live for the whole program — static storage.
namespace {
kairo::Esp32Platform  platform;
kairo::DevBoard       board;
kairo::Runtime        rt = kairo::Runtime::create();

kairo::ClockService*    clockSvc = nullptr;
kairo::HelloPlugin      helloPlugin;
kairo::ClockPlugin      clockPlugin;
kairo::CounterPlugin    counterPlugin;
kairo::StopwatchPlugin  stopwatchPlugin;
kairo::TaskDemoPlugin   taskDemoPlugin;
kairo::TickerPlugin     tickerPlugin;
kairo::HomeScreen*      homeScreen = nullptr;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[kairo] booting dev-board...");

    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    // Core background service
    static kairo::ClockService cs(rt.log(), rt.events());
    clockSvc = &cs;
    rt.container().registerService(clockSvc);

    rt.start();

    // Plugins + home screen
    rt.plugins().load(helloPlugin);
    rt.plugins().load(clockPlugin);
    rt.plugins().load(counterPlugin);
    rt.plugins().load(stopwatchPlugin);
    rt.plugins().load(taskDemoPlugin);
    rt.plugins().load(tickerPlugin);
    static kairo::HomeScreen hs(rt);
    homeScreen = &hs;
    rt.view().push(*homeScreen);

    // Logger exists now → use rt.log(), not raw Serial (see CLAUDE.md).
    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();   // one iteration: tick services/plugins, poll buttons, redraw e-ink
}
