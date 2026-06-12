// Palanu Dev Board — ESP32-S3 + e-ink entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
// Same boot flow as the simulator target; only Platform + Board differ.
#include <Arduino.h>

#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/esp32/esp32_platform.h"
#include "nema/devboard/dev_board.h"
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

// Runtime objects live for the whole program — static storage.
namespace {
nema::Esp32Platform  platform;
nema::DevBoard       board;
nema::Runtime        rt = nema::Runtime::create();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[nema] booting dev-board...");

    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    // Background service shipped via the same registry as apps (AppType::
    // Service, hidden from the launcher). Installed before start() → boots
    // with the system.
    static nema::ClockService cs(rt.log(), rt.events());
    rt.apps().installService(cs, "com.palanu.svc.clock");

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

    static nema::HomeScreen hs(rt);
    rt.view().push(hs);

    // Logger exists now → use rt.log(), not raw Serial (see CLAUDE.md).
    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();   // one iteration: tick services, poll buttons, redraw e-ink
}
