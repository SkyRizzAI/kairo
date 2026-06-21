#include "nema/ui/component_screen.h"
#include "nema/ui/canvas.h"
#include "nema/ui/text_style.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/runtime.h"

namespace nema {

void ComponentScreen::requestRedraw() { rt_.view().requestRedraw(); }

void ComponentScreen::enter() {
    state_.modality = input::InputModality::Button;
    dirty_ = true;   // force tree rebuild on next draw (Plan 70)
    requestRedraw();
}

void ComponentScreen::onResume() {
    state_.modality = input::InputModality::Button;
    dirty_ = true;
    requestRedraw();
}

void ComponentScreen::draw(Canvas& c) {
    // Plan 70 FPS opt: only rebuild the tree if model data changed.
    // Layout + render always runs (scroll momentum, marquee, ellipsis
    // need fresh positional data even when the tree is unchanged).
    if (dirty_) {
        arena_.reset();
        state_.pressed = nullptr;          // arena reset invalidates node pointers
        root_ = build(arena_, rt_);
        dirty_ = false;
    }
    if (!root_) return;
    uint16_t w = c.width();
    uint16_t h = c.height();
    // Normal mode leaves the top strip for the status bar (drawn by GuiService);
    // fullscreen screens own the whole canvas.
    int16_t  oy = fullscreen() ? 0 : (int16_t)nema::display::contentY();
    uint16_t ah = fullscreen() ? h : (uint16_t)(h - nema::display::contentY());
    aether::ui::renderComponentFrame(root_, c, state_, aether::ui::roleMetrics(), 0, oy, w, ah);
}

void ComponentScreen::onAction(input::Action a) {
    if (!root_) return;
    bool changed = false;
    using A = input::Action;
    switch (a) {
        // Up/Down = primary vertical navigation (Plan 70 fix)
        case A::Prev:       changed = aether::ui::dispatchNav(root_, state_, aether::ui::Nav::Prev);     break;
        case A::Next:       changed = aether::ui::dispatchNav(root_, state_, aether::ui::Nav::Next);     break;
        case A::Activate:   changed = aether::ui::dispatchNav(root_, state_, aether::ui::Nav::Activate); break;
        // Left/Right: fine-adjust a focused value control (slider/stepper); if the
        // focused node isn't adjustable, fall back to moving focus.
        case A::AdjustUp:   changed = aether::ui::dispatchAdjust(root_, state_, +1) ||
                                       aether::ui::dispatchNav(root_, state_, aether::ui::Nav::Next);     break;
        case A::AdjustDown: changed = aether::ui::dispatchAdjust(root_, state_, -1) ||
                                       aether::ui::dispatchNav(root_, state_, aether::ui::Nav::Prev);     break;
        case A::Back:
            // Plan 70: try IScreen::onBackPressed first (new), then ComponentScreen::onBack (legacy)
            if (!onBackPressed() && !onBack()) rt_.view().goBack();
            return;
        default: break;
    }
    if (changed) {
        dirty_ = true;   // Plan 70: interaction may have changed model data
        requestRedraw();
    }
}

void ComponentScreen::onPointer(const input::PointerEvent& e) {
    if (!root_) return;
    if (aether::ui::dispatchPointer(root_, state_, e)) {
        dirty_ = true;   // Plan 70: pointer interaction may have changed model data
        requestRedraw();
    }
}

void ComponentScreen::tick(uint64_t nowMs) {
    bool dirty = false;
    // Drive flick momentum (GuiService ticks every loop ~10ms).
    if (state_.dragScroll && state_.dragScroll->velocity != 0.0f)
        dirty = aether::ui::tickMomentum(state_);
    // Marquee animation: rate-limited to ~15fps (66ms) so the display doesn't
    // flicker from continuous full-speed redraws. Marquee speed itself is
    // controlled by the tick/25 divisor in draw::marquee (~40px/sec).
    if (state_.focus.count > 0 && (nowMs - lastMarqueeMs_) >= 66) {
        lastMarqueeMs_ = nowMs;
        dirty = true;
    }
    if (dirty) requestRedraw();
}

} // namespace nema
