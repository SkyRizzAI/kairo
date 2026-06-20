// Palanu Dev Board — ESP32-S3 + e-ink entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
#include <Arduino.h>

#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/esp32/esp32_platform.h"
#include "nema/devboard/dev_board.h"
#include "nema/services/clock_service.h"
#include "nema/app/app_registry.h"
#include "nema/apps/js_app_store.h"
#include "nema/apps/dolphin_app.h"
#include "nema/screens/home_screen.h"
#include "nema/ui/view_dispatcher.h"

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

    static nema::ClockService cs(rt.log(), rt.events());
    rt.apps().installService(cs, "com.palanu.svc.clock");

    rt.start();

    static nema::DolphinApp dolphinApp;
    rt.apps().install(dolphinApp);

    nema::loadEmbeddedJsApps(rt);

    static nema::HomeScreen hs(rt);
    rt.view().push(hs);

    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();
}
