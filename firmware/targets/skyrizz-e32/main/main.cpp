// SkyRizz E32 — ESP32-S3 + TFT LCD + XL9535 3-button entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
#include <Arduino.h>

#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/esp32/esp32_platform.h"
#include "kairo/skyrizze32/skyrizz_e32.h"
#include "kairo/service/service_container.h"
#include "kairo/services/clock_service.h"
#include "kairo/plugins/hello_plugin.h"
#include "kairo/plugins/clock_plugin.h"
#include "kairo/plugins/counter_plugin.h"
#include "kairo/plugins/stopwatch_plugin.h"
#include "kairo/plugins/task_demo_plugin.h"
#include "kairo/plugins/ticker_plugin.h"
#include "kairo/plugins/camera_plugin.h"
#include "kairo/plugins/ui_showcase_plugin.h"
#include "kairo/plugin/plugin_manager.h"
#include "kairo/screens/home_screen.h"
#include "kairo/ui/view_dispatcher.h"

namespace {
kairo::Esp32Platform             platform;
kairo::skyrizze32::SkyRizzE32   board;
kairo::Runtime                   rt = kairo::Runtime::create();

kairo::HelloPlugin      helloPlugin;
kairo::ClockPlugin      clockPlugin;
kairo::CounterPlugin    counterPlugin;
kairo::StopwatchPlugin  stopwatchPlugin;
kairo::TaskDemoPlugin   taskDemoPlugin;
kairo::TickerPlugin     tickerPlugin;
kairo::CameraPlugin     cameraPlugin;
kairo::UiShowcasePlugin uiShowcasePlugin;
kairo::HomeScreen*      homeScreen = nullptr;
}

void setup() {
    // USB CDC console (UART0 GPIO43/44 occupied by XL9535 INT + SPI clock)
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[kairo] booting skyrizz-e32...");

    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    static kairo::ClockService cs(rt.log(), rt.events());
    rt.container().registerService(&cs);

    rt.start();

    rt.plugins().load(helloPlugin);
    rt.plugins().load(clockPlugin);
    rt.plugins().load(counterPlugin);
    rt.plugins().load(stopwatchPlugin);
    rt.plugins().load(taskDemoPlugin);
    rt.plugins().load(tickerPlugin);
    rt.plugins().load(cameraPlugin);
    rt.plugins().load(uiShowcasePlugin);

    static kairo::HomeScreen hs(rt);
    homeScreen = &hs;
    rt.view().push(*homeScreen);

    // Logger exists now → use rt.log(), not raw Serial (see CLAUDE.md).
    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();
}
