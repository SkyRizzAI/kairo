// SkyRizz E32 — ESP32-S3 + TFT LCD + XL9535 3-button entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
#include <Arduino.h>

#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/esp32/esp32_platform.h"
#include "nema/skyrizze32/skyrizz_e32.h"
#include "nema/services/clock_service.h"
#include "nema/app/app_registry.h"
#include "nema/apps/js_app_store.h"
#include "nema/apps/hello_app.h"
#include "nema/screens/home_screen.h"
#include "nema/ui/view_dispatcher.h"

namespace {
nema::Esp32Platform            platform;
nema::skyrizze32::SkyRizzE32   board;
nema::Runtime                  rt = nema::Runtime::create();
}

void setup() {
    nema::usbSerialJtag().begin();
    delay(200);
    nema::usbSerialJtag().println("\n[nema] booting skyrizz-e32...");

    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    static nema::ClockService cs(rt.log(), rt.events());
    rt.apps().installService(cs, "com.palanu.svc.clock");

    rt.start();

    static nema::HelloApp helloApp;
    rt.apps().install(helloApp);

    nema::loadEmbeddedJsApps(rt);

    static nema::HomeScreen hs(rt);
    rt.view().push(hs);

    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();
}
