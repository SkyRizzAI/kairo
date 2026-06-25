#include <emscripten.h>
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/wasm/wasm_platform.h"
#include "nema/sim/simulator_board.h"
#include "nema/services/clock_service.h"
#include "nema/app/app_registry.h"
#include "nema/apps/bad_usb_app.h"
#include "nema/apps/wallets_app.h"
#include "nema/wallet/wallet_system.h"
#include "aether/screens/desktop_screen.h"
#include "nema/ui/view_dispatcher.h"
#include "aether/boot.h"

namespace {
    nema::WasmPlatform           platform;
    nema::SimulatorBoard         board;
    nema::Runtime                rt = nema::Runtime::create();
}

static void loop() { rt.step(); }

int main() {
    rt.loadPlatform(platform);
    rt.loadBoard(board);
    rt.initCore();
    rt.registerServices();

    static nema::ClockService clockSvc(rt.log(), rt.events());
    rt.apps().installService(clockSvc, "com.palanu.svc.clock");

    rt.start();
    nema::wallet::bootWalletSystem(rt);   // shared wallet (Wallets app + nema.wallet.*)
    aether::bootDisplay(rt);   // Plan 80: construct + start display servers + GUI loop

    static nema::BadUsbApp badUsbApp;
    rt.apps().install(badUsbApp, "1.0.0");

    static nema::WalletsApp walletsApp;   // Plan 94 — crypto wallet
    rt.apps().install(walletsApp, "1.0.0");

    static nema::DesktopScreen desktop(rt);   // Plan 81: idle wallpaper → launcher
    rt.view().push(desktop);

    aether::bootSplash(rt);   // cat logo + 2s progress bar on top → reveals desktop

    rt.log().info("Boot", "wasm ready");

    emscripten_set_main_loop(loop, 0, 1);
    return 0;
}
