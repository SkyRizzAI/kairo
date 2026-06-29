// SkyRizz Solana — ESP32-S3 + ILI9341 TFT + TCA9534 6-button D-pad entry point.
// Arduino-as-component: setup() runs once, loop() runs repeatedly.
#include <Arduino.h>

#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/esp32/esp32_platform.h"
#include "nema/skyrizzsolana/skyrizz_solana.h"
#include "nema/services/clock_service.h"
#include "nema/app/app_registry.h"
#include "nema/app/papp_installer.h"
#include "nema/apps/bad_usb_app.h"
#include "nema/apps/wallets_app.h"
#include "nema/wallet/wallet_system.h"
#include "aether/screens/desktop_screen.h"
#include "nema/ui/view_dispatcher.h"
#include "aether/boot.h"

namespace {
nema::Esp32Platform              platform;
nema::skyrizzsolana::SkyRizzSolana board;
nema::Runtime                    rt = nema::Runtime::create();
}

void setup() {
    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    static nema::ClockService cs(rt.log(), rt.events());
    rt.apps().installService(cs, "com.palanu.svc.clock");

    rt.start();
    nema::wallet::bootWalletSystem(rt);   // shared wallet for the Wallets app + nema.wallet.*
    nema::loadInstalledPapps(rt);  // scan /system/apps + /sd/apps for user-installed .papp
    aether::bootDisplay(rt);   // Plan 80: construct + start display servers + GUI loop

    static nema::BadUsbApp badUsbApp;
    rt.apps().install(badUsbApp, "1.0.0");

    static nema::WalletsApp walletsApp;   // Plan 94 — crypto wallet
    rt.apps().install(walletsApp, "1.0.0");

    static nema::DesktopScreen desktop(rt);   // Plan 81: idle wallpaper → launcher
    rt.view().push(desktop);

    aether::bootSplash(rt);   // cat logo + 2s progress bar on top → reveals desktop

    rt.log().info("Boot", "ready");
}

void loop() {
    rt.step();
}
