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
#include "nema/apps/dolphin_app.h"
#include "aether/screens/desktop_screen.h"
#include "nema/ui/view_dispatcher.h"
#include "aether/boot.h"

namespace {
nema::Esp32Platform            platform;
nema::skyrizze32::SkyRizzE32   board;
nema::Runtime                  rt = nema::Runtime::create();
}

void setup() {
    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    static nema::ClockService cs(rt.log(), rt.events());
    rt.apps().installService(cs, "com.palanu.svc.clock");

    rt.start();
    aether::bootDisplay(rt);   // Plan 80: construct + start display servers + GUI loop

    static nema::HelloApp helloApp;
    rt.apps().install(helloApp);

    static nema::DolphinApp dolphinApp;
    rt.apps().install(dolphinApp);

    nema::loadEmbeddedJsApps(rt);

    static nema::DesktopScreen desktop(rt);   // Plan 81: idle wallpaper → launcher
    rt.view().push(desktop);

    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();
}
