// SkyRizz E32 — ESP32-S3 + TFT LCD + XL9535 3-button entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
#include <Arduino.h>

#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/esp32/esp32_platform.h"
#include "nema/skyrizze32/skyrizz_e32.h"
#include "nema/services/clock_service.h"
#include "nema/app/app_registry.h"
#include "nema/apps/clock_app.h"
#include "nema/apps/counter_app.h"
#include "nema/apps/stopwatch_app.h"
#include "nema/apps/task_demo_app.h"
#include "nema/apps/ticker_app.h"
#include "nema/apps/camera_app.h"
#include "nema/apps/ui_showcase_app.h"
#include "nema/apps/js_app_store.h"
#include "nema/screens/home_screen.h"
#include "nema/ui/view_dispatcher.h"

namespace {
nema::Esp32Platform            platform;
nema::skyrizze32::SkyRizzE32   board;
nema::Runtime                  rt = nema::Runtime::create();
}

void setup() {
    // Boot banner on the USB-Serial-JTAG console. NOT `Serial`: under
    // arduino-as-component that is UART0 (GPIO43/44 — taken by XL9535 INT + SD
    // clock), and begin() would install a UART driver on those pins. The native
    // USB port is Palanu's shared HWCDC instance (see esp32_usb_cdc.h).
    nema::usbSerialJtag().begin();
    delay(200);
    nema::usbSerialJtag().println("\n[nema] booting skyrizz-e32...");

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

    // Install the built-in apps (they appear in the launcher; selecting one
    // spawns it on its own thread). Camera is a UI-thread screen, not a thread.
    static nema::ClockApp       clockApp;
    static nema::CounterApp     counterApp;
    static nema::StopwatchApp   stopwatchApp;
    static nema::TaskDemoApp    taskDemoApp;
    static nema::TickerApp      tickerApp;
    static nema::UiShowcaseApp  uiShowcaseApp;
    static nema::CameraApp      cameraApp(rt);
    rt.apps().install(clockApp);
    rt.apps().install(counterApp);
    rt.apps().install(stopwatchApp);
    rt.apps().install(taskDemoApp);
    rt.apps().install(tickerApp);
    rt.apps().install(uiShowcaseApp);
    rt.apps().installScreen("com.palanu.camera", "Camera", cameraApp);
    nema::loadEmbeddedJsApps(rt);   // built-in JS (custom) apps (Plan 37)

    static nema::HomeScreen hs(rt);
    rt.view().push(hs);

    // Logger exists now → use rt.log(), not raw Serial (see CLAUDE.md).
    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();
}
