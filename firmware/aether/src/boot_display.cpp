#include "aether/boot.h"
#include "nema/runtime.h"
#include "nema/services/gui_service.h"
#include "nema/app/app_host_manager.h"
#include "nema/screens/close_and_open_modal.h"
#include "nema/ui/screen.h"
#include "nema/ui/aether_server.h"
#include "fbcon/fbcon_server.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/canvas.h"
#include "nema/config/config_store.h"
#include "nema/service/service_container.h"
#include <memory>
#include <string>

namespace aether {

void bootDisplay(nema::Runtime& rt) {
    // Owned for the process lifetime — the GUI loop and registry hold pointers.
    static nema::AetherServer aetherSrv(rt.clock());
    static fbcon::FbconServer  fbconSrv(rt);
    static nema::GuiService   gui(rt);

    // Aether owns its presentational state (theme + scale + FPS overlay); load it
    // from config. FbCon is a text console — always scale 1, no theme. CLI-first
    // boot (config display/boot, default "fbcon") lands in the console; set
    // "aether" to boot straight into the UI.
    bool bootAether = false;
    if (auto* cfg = rt.container().resolve<nema::IConfigStore>()) {
        aetherSrv.setShowFps(cfg->getIntOr("debug", "fps", 0) != 0);
        std::string t = cfg->getString("display", "theme", "default");
        if (t == "compact")     aetherSrv.setTheme(aether::compactTheme());
        else if (t == "large")  aetherSrv.setTheme(aether::largeTheme());
        else                    aetherSrv.setTheme(aether::defaultTheme());
        if (rt.canvas().scale() >= 1.0f) aetherSrv.setServerScale(rt.canvas().scale());
        bootAether = (cfg->getString("display", "boot", "fbcon") == "aether");
    }

    // Register backends with the kernel registry (Plan 80). The boot flag picks
    // the initially-active one; runtime swaps go through rt.switchDisplayServer().
    rt.registerDisplayServer(&fbconSrv, !bootAether);
    rt.registerDisplayServer(&aetherSrv, bootAether);

    // Install the app-switch transition modal ("Close & Open?") — a presentation
    // screen, so the kernel app manager gets it via this factory (Plan 80).
    rt.appHost().setTransitionModalFactory(
        [](nema::Runtime& r, nema::AppHostManager& m, nema::IApp& a)
            -> std::unique_ptr<nema::IScreen> {
            return std::make_unique<nema::CloseAndOpenModal>(r, m, a);
        });

    gui.start();   // spawn the UI thread (renders rt.displayServer())
}

}  // namespace aether
