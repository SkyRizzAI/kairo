// Plan 81 — DesktopScreen implementation.
#include "nema/screens/desktop_screen.h"
#include "nema/shell/shell_factory.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/animation_manager.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/input/input_action.h"
#include "nema/runtime.h"
#include "nema/config/config_store.h"
#include <string>

namespace nema {

DesktopScreen::DesktopScreen(Runtime& rt)
    : ComponentScreen(rt), launcher_(rt) {}

void DesktopScreen::onResume() {
    ComponentScreen::onResume();
    statusBar_ = rt_.config().getIntOr("display", "statusbar", 1) != 0;

    std::string name = rt_.config().getString("display", "desktop", shell::kDefaultDesktop);
    if (!theme_ || name != theme_->name()) {
        if (theme_)
            if (auto* p = theme_->player())
                anim::AnimationManager::instance().unregisterPlayer(*p);
        theme_ = shell::makeDesktop(name.c_str(), rt_);
    }
    if (theme_) {
        theme_->onResume();
        if (auto* p = theme_->player()) {
            auto& mgr = anim::AnimationManager::instance();
            mgr.unregisterPlayer(*p);   // idempotent — guarantee exactly one entry
            mgr.registerPlayer(*p);
        }
    }
    requestRedraw();
}

void DesktopScreen::onPause() {
    if (theme_)
        if (auto* p = theme_->player())
            anim::AnimationManager::instance().unregisterPlayer(*p);
}

void DesktopScreen::draw(Canvas& c) {
    if (!theme_) return;
    uint16_t W = c.width(), H = c.height();
    uint16_t cy = nema::display::contentY();
    theme_->draw(c, 0, cy, W, (uint16_t)(H > cy ? H - cy : 0));
}

void DesktopScreen::onAction(input::Action a) {
    // OK opens the launcher; everything else (incl. Back) is ignored — this is home.
    if (a == input::Action::Activate)
        rt_.view().navigate(launcher_);
}

aether::ui::UiNode* DesktopScreen::build(aether::ui::NodeArena&, Runtime&) {
    return nullptr;   // skin paints directly in draw()
}

} // namespace nema
