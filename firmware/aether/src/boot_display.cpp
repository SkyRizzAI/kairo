#include "aether/boot.h"
#include "nema/runtime.h"
#include "nema/services/gui_service.h"
#include "nema/app/app_host_manager.h"
#include "nema/screens/close_and_open_modal.h"
#include "nema/screens/splash_screen.h"
#include "nema/ui/view_dispatcher.h"
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
        aetherSrv.setShowFps(cfg->getIntOr("aether", "fps", 0) != 0);
        // Size stays default; "aether/theme" now selects the COLOUR theme (Plan 92
        // Fase B): default = mono (white/black), flipper = orange/black. Same fonts.
        aetherSrv.setTheme(aether::defaultTheme());
        std::string t = cfg->getString("aether", "theme", "default");
        aether::setColorTheme(t == "flipper" ? aether::flipperColors()
                                             : aether::monoColors());
        aether::setDarkMode(cfg->getIntOr("aether", "dark", 0) != 0);
        if (rt.canvas().scale() >= 1.0f) aetherSrv.setServerScale(rt.canvas().scale());
        bootAether = (cfg->getString("display", "boot", "aether") == "aether");
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

void bootSplash(nema::Runtime& rt) {
    // Owned here (aether), not by any target/board. Pushed on top of the desktop;
    // pops itself after 2s (SplashScreen::Mode::Boot).
    static nema::SplashScreen splash(rt);
    rt.view().push(splash);
}

}  // namespace aether
