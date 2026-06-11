// Kairo Dev Board — ESP32-S3 + e-ink entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
// Same boot flow as the simulator target; only Platform + Board differ.
#include <Arduino.h>

#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/esp32/esp32_platform.h"
#include "kairo/devboard/dev_board.h"
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

// Runtime objects live for the whole program — static storage.
namespace {
kairo::Esp32Platform  platform;
kairo::DevBoard       board;
kairo::Runtime        rt = kairo::Runtime::create();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[kairo] booting dev-board...");

    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    // Background service shipped via the same registry as apps (AppType::
    // Service, hidden from the launcher). Installed before start() → boots
    // with the system.
    static kairo::ClockService cs(rt.log(), rt.events());
    rt.apps().installService(cs, "com.kairo.svc.clock");

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

    static kairo::HomeScreen hs(rt);
    rt.view().push(hs);

    // Logger exists now → use rt.log(), not raw Serial (see CLAUDE.md).
    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();   // one iteration: tick services, poll buttons, redraw e-ink
}
