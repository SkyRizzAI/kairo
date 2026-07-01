#include "nema/ui/component_screen.h"
#include "nema/ui/canvas.h"
#include "nema/ui/text_style.h"
#include "nema/ui/draw.h"
#include "nema/ui/renderer.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/runtime.h"
#include "nema/task_runner.h"

namespace nema {

void ComponentScreen::requestRedraw() { rt_.view().requestRedraw(); }

void ComponentScreen::runBusy(const char* label, std::function<void()> work,
                              std::function<void()> done) {
    if (busy_) return;                       // ignore re-entry — anti double-action
    busy_      = true;
    busyLabel_ = label;
    requestRedraw();                         // show the overlay immediately
    rt_.tasks().submit(std::move(work), [this, done] {
        busy_  = false;                      // Done runs on the UI thread
        dirty_ = true;                        // the op changed model state → rebuild the tree
        if (done) done();
        requestRedraw();
    });
}

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

    if (mode() == ScreenMode::Modal) {
        // Modal: layout + render inside the centered box so content aligns with
        // the white box AetherServer already drew. Without this, renderComponentFrame
        // uses (0, contentY(), fullWidth, …) and text renders above the modal box.
        //
        // Clamp to canvas size: if modalWidth() > canvas width (e.g. a 210px modal on
        // a 128px display), the layout must use the actual canvas width, not the declared
        // modal width — otherwise layout centers content in a 210px virtual space and
        // text renders beyond the right edge of the canvas.
        uint16_t mw = (modalWidth()  < w) ? modalWidth()  : w;
        uint16_t mh = (modalHeight() < h) ? modalHeight() : h;
        uint16_t mx = (w > mw) ? (uint16_t)((w - mw) / 2) : 0;
        uint16_t my = (h > mh) ? (uint16_t)((h - mh) / 2) : 0;
        aether::ui::renderComponentFrame(root_, c, state_, aether::ui::roleMetrics(),
                                         (int16_t)mx, (int16_t)my, mw, mh);
        return;
    }

    // Normal mode leaves the top strip for the status bar (drawn by GuiService);
    // fullscreen screens own the whole canvas.
    int16_t  oy = fullscreen() ? 0 : (int16_t)nema::display::contentY();
    uint16_t ah = fullscreen() ? h : (uint16_t)(h - nema::display::contentY());
    aether::ui::renderComponentFrame(root_, c, state_, aether::ui::roleMetrics(), 0, oy, w, ah);

    // Flipper-style busy overlay: centered spinner + label drawn on top while a runBusy()
    // op is in flight. Input is ignored meanwhile (onAction/onPointer), so no double-action.
    if (busy_) {
        uint16_t bw = 96; if (bw > w) bw = w;
        uint16_t bh = 38; if (bh > h) bh = h;
        uint16_t bx = (uint16_t)((w - bw) / 2);
        uint16_t by = (uint16_t)((h - bh) / 2);
        c.fillRect(bx, by, bw, bh, false);                  // clear behind the box
        c.drawRoundRect(bx, by, bw, bh, 3, true);           // outline
        aether::ui::draw::spinner(c, (uint16_t)(bx + bw / 2), (uint16_t)(by + 14), 5,
                                  aether::ui::renderTick());
        if (busyLabel_) {
            aether::ui::FontSpec fs = aether::ui::fontForRole(aether::ui::TextRole::Body);
            c.setFont(fs.handle);
            uint16_t tw = aether::ui::measureTextW(busyLabel_, aether::ui::TextRole::Body);
            uint16_t tx = (uint16_t)(bx + (bw > tw ? (bw - tw) / 2 : 0));
            c.drawText(tx, (uint16_t)(by + bh - 12), busyLabel_, true);
        }
    }
}

void ComponentScreen::onAction(input::Action a) {
    if (busy_) return;          // busy overlay swallows input — anti double-action
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
    if (busy_) return;          // busy overlay swallows input
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
    // flicker from continuous full-speed redraws. Marquee scroll speed itself is
    // controlled by MARQUEE_MS_PER_PX in draw::marquee (~25px/sec).
    if (state_.focus.count > 0 && (nowMs - lastMarqueeMs_) >= 66) {
        lastMarqueeMs_ = nowMs;
        dirty = true;
    }
    // Animate the busy spinner while an op is in flight (~15fps).
    if (busy_ && (nowMs - lastMarqueeMs_) >= 66) {
        lastMarqueeMs_ = nowMs;
        dirty = true;
    }
    if (dirty) requestRedraw();
}

} // namespace nema
