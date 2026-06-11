// SkyRizz E32 — ESP32-S3 + TFT LCD + XL9535 3-button entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
#include <Arduino.h>

#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/esp32/esp32_platform.h"
#include "kairo/skyrizze32/skyrizz_e32.h"
#include "kairo/services/clock_service.h"
#include "kairo/app/app_registry.h"
#include "kairo/apps/clock_app.h"
#include "kairo/apps/counter_app.h"
#include "kairo/apps/stopwatch_app.h"
#include "kairo/apps/task_demo_app.h"
#include "kairo/apps/ticker_app.h"
#include "kairo/apps/camera_app.h"
#include "kairo/apps/ui_showcase_app.h"
#include "kairo/apps/js_app_store.h"
#include "kairo/screens/home_screen.h"
#include "kairo/ui/view_dispatcher.h"

namespace {
kairo::Esp32Platform            platform;
kairo::skyrizze32::SkyRizzE32   board;
kairo::Runtime                  rt = kairo::Runtime::create();
}

void setup() {
    // Boot banner on the USB-Serial-JTAG console. NOT `Serial`: under
    // arduino-as-component that is UART0 (GPIO43/44 — taken by XL9535 INT + SD
    // clock), and begin() would install a UART driver on those pins. The native
    // USB port is Kairo's shared HWCDC instance (see esp32_usb_cdc.h).
    kairo::usbSerialJtag().begin();
    delay(200);
    kairo::usbSerialJtag().println("\n[kairo] booting skyrizz-e32...");

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

    // Install the built-in apps (they appear in the launcher; selecting one
    // spawns it on its own thread). Camera is a UI-thread screen, not a thread.
    static kairo::ClockApp       clockApp;
    static kairo::CounterApp     counterApp;
    static kairo::StopwatchApp   stopwatchApp;
    static kairo::TaskDemoApp    taskDemoApp;
    static kairo::TickerApp      tickerApp;
    static kairo::UiShowcaseApp  uiShowcaseApp;
    static kairo::CameraApp      cameraApp(rt);
    rt.apps().install(clockApp);
    rt.apps().install(counterApp);
    rt.apps().install(stopwatchApp);
    rt.apps().install(taskDemoApp);
    rt.apps().install(tickerApp);
    rt.apps().install(uiShowcaseApp);
    rt.apps().installScreen("com.kairo.camera", "Camera", cameraApp);
    kairo::loadEmbeddedJsApps(rt);   // built-in JS (custom) apps (Plan 37)

    static kairo::HomeScreen hs(rt);
    rt.view().push(hs);

    // Logger exists now → use rt.log(), not raw Serial (see CLAUDE.md).
    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();
}
