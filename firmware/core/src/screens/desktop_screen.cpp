// Plan 81 — DesktopScreen implementation.
#include "aether/screens/desktop_screen.h"
#include "aether/shell/shell_factory.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/animation_manager.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/widgets.h"
#include "nema/ui/component_runtime.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/icon_pack.h"
#include "nema/input/input_action.h"
#include "nema/runtime.h"
#include "nema/services/display_power_manager.h"
#include "nema/config/config_store.h"
#include <string>

namespace nema {

DesktopScreen::DesktopScreen(Runtime& rt)
    : ComponentScreen(rt), launcher_(rt), missionControl_(rt) {}

void DesktopScreen::onResume() {
    ComponentScreen::onResume();
    statusBar_ = rt_.config().getIntOr("aether", "statusbar", 1) != 0;

    std::string name = rt_.config().getString("aether", "desktop", shell::kDefaultDesktop);
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
    // Re-arm the footer legends: show full labels, then collapse after the delay.
    footerAnim_.reset(2);
    lastTickMs_ = 0;
    requestRedraw();
}

void DesktopScreen::tick(uint64_t nowMs) {
    ComponentScreen::tick(nowMs);

    // Freeze the footer timer while the display is off (sleep / locked-pre-wake),
    // and replay the intro on wake: returning to a lit screen should show the
    // labels again and re-collapse — same as coming back from the launcher
    // (which fires onResume). Plain sleep→wake does NOT navigate, so detect the
    // display-off→on edge here.
    if (rt_.dpm().isDisplayOff()) {
        wasDisplayOff_ = true;
        lastTickMs_    = 0;        // drop the stale dt baseline
        return;
    }
    if (wasDisplayOff_) {
        wasDisplayOff_ = false;
        footerAnim_.reset(2);     // woke → icon+label again, collapse after the delay
        lastTickMs_    = 0;
    }

    float dt = lastTickMs_ ? (float)(nowMs - lastTickMs_) : 0.f;
    lastTickMs_ = nowMs;
    if (footerAnim_.tick(dt)) requestRedraw();   // keep frames coming while collapsing
}

void DesktopScreen::onPause() {
    if (theme_)
        if (auto* p = theme_->player())
            anim::AnimationManager::instance().unregisterPlayer(*p);
}

// Footer legend overlay (Plan 92). Two pill hints painted over the wallpaper:
//   • left  — Up (Prev) opens Mission Control  → up-chevron + "Mission Control"
//             (falls back to "Missions" when the canvas is too narrow to fit both)
//   • right — OK (Activate) opens the Launcher → ↵ enter icon + "Launcher"
// Pure hints: input is handled by onAction(), so the pills are not focus stops.
void DesktopScreen::drawFooterLegends(Canvas& c, uint16_t W, uint16_t H) {
    using namespace aether::ui;
    const uint8_t sm = aether::theme().space.sm;   // bottom/edge inset for the bar

    LegendItem items[2] = {};
    if (const IconDef* d = findIcon("nav.up")) {
        items[0].icon = d->bitmap; items[0].iconW = d->w; items[0].iconH = d->h;
    }
    if (const IconDef* d = findIcon("nav.enter")) {   // ↵ enter/return = OK/Activate
        items[1].icon = d->bitmap; items[1].iconW = d->w; items[1].iconH = d->h;
    }
    items[1].label = "Launcher";

    // Natural pill width = padding both sides + icon + (gap + label) when labelled.
    auto pillW = [&](const char* label, uint8_t iconW) -> uint16_t {
        uint16_t lw = (label && *label)             // gap(1) + label text
            ? (uint16_t)(1 + measureTextW(label, TextRole::Mono)) : 0;
        return (uint16_t)(2 * 1 + iconW + lw);      // pill padding is 1px both sides
    };
    // Prefer the full label; shorten to "Missions" when both pills + edge insets
    // wouldn't fit the canvas width with a sensible gap between them.
    const char* leftLabel = "Mission Control";
    uint16_t need = (uint16_t)(pillW(leftLabel, items[0].iconW) +
                              pillW(items[1].label, items[1].iconW) + 2 * sm + 8);
    if (need > W) leftLabel = "Missions";
    items[0].label = leftLabel;

    // Build a bottom-anchored, full-width row and paint it over the wallpaper.
    arena_.reset();
    UiNode* legends = FooterLegends(arena_, items, 2, footerAnim_);
    Style col; col.dir = FlexDir::Col; col.align = Align::Stretch;
    col.justify = Justify::End; col.padding = sm;
    UiNode* root = View(arena_, col, { legends });

    uint16_t cy = nema::display::contentY();
    uint16_t ch = (uint16_t)(H > cy ? H - cy : 0);
    renderComponentFrame(root, c, state_, roleMetrics(), 0, (int16_t)cy, W, ch);
}

void DesktopScreen::draw(Canvas& c) {
    if (!theme_) return;
    uint16_t W = c.width(), H = c.height();
    uint16_t cy = nema::display::contentY();
    theme_->draw(c, 0, cy, W, (uint16_t)(H > cy ? H - cy : 0));
    drawFooterLegends(c, W, H);
}

void DesktopScreen::onAction(input::Action a) {
    // OK opens the launcher. Up (Prev) or Left (AdjustDown) opens Mission Control —
    // the Flipper-style quick-settings panel. Back is ignored (this is home).
    if (a == input::Action::Activate)
        rt_.view().navigate(launcher_);
    else if (a == input::Action::Prev || a == input::Action::AdjustDown)
        rt_.view().navigate(missionControl_);
}

aether::ui::UiNode* DesktopScreen::build(aether::ui::NodeArena&, Runtime&) {
    return nullptr;   // skin paints directly in draw()
}

} // namespace nema
