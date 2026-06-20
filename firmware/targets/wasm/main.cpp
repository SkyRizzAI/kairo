#include <emscripten.h>
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/wasm/wasm_platform.h"
#include "nema/sim/simulator_board.h"
#include "nema/services/clock_service.h"
#include "nema/app/app_registry.h"
#include "nema/apps/js_app_store.h"
#include "nema/apps/dolphin_app.h"
#include "nema/screens/home_screen.h"
#include "nema/ui/view_dispatcher.h"

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

    static nema::DolphinApp dolphinApp;
    rt.apps().install(dolphinApp);

    nema::loadEmbeddedJsApps(rt);

    static nema::HomeScreen homeScreen(rt);
    rt.view().push(homeScreen);

    rt.log().info("Boot", "wasm ready");

    emscripten_set_main_loop(loop, 0, 1);
    return 0;
}
