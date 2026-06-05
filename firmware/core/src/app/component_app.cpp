#include "kairo/app/component_app.h"
#include "kairo/app/app_context.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/component_runtime.h"
#include "kairo/ui/text_style.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/nema/input_event.h"

namespace kairo {

// Map a physical Key to component navigation; returns true if it moved focus or
// activated. Up/Left = Prev, Down/Right = Next, Select = Activate.
static bool navFromKey(Key k, ui::UiNode* root, ui::ComponentState& st) {
    switch (k) {
        case Key::Up:     return ui::dispatchNav(root, st, ui::Nav::Prev);
        case Key::Down:   return ui::dispatchNav(root, st, ui::Nav::Next);
        case Key::Select: return ui::dispatchNav(root, st, ui::Nav::Activate);
        // Left/Right: adjust a focused value control first, else move focus.
        case Key::Left:   return ui::dispatchAdjust(root, st, -1) || ui::dispatchNav(root, st, ui::Nav::Prev);
        case Key::Right:  return ui::dispatchAdjust(root, st, +1) || ui::dispatchNav(root, st, ui::Nav::Next);
        default:          return false;
    }
}

void ComponentApp::run(AppContext& ctx) {
    ui::NodeArena arena(arenaCapacity());
    ui::UiNode* root = nullptr;
    bool dirty = true;

    // Shared interaction state (Plan 30 runtime): focus + dual modality +
    // tap/drag/momentum. ComponentRuntime drives layout, scroll auto-follow,
    // hit-testing and gesture recognition identically for apps and screens.
    ui::ComponentState st;

    onStart(ctx);

    while (!ctx.shouldExit()) {
        if (dirty) {
            Canvas& c = ctx.canvas();
            c.clear();
            if (drawRaw(c, ctx)) {
                root = nullptr;   // custom-drawn frame (e.g. keyboard)
            } else {
                arena.reset();
                st.pressed = nullptr;   // arena reset invalidates node pointers
                root = build(arena, ctx);
                if (root) {
                    uint16_t w = c.width();
                    uint16_t h = c.height();
                    // Normal mode leaves the top strip for the system status bar.
                    int16_t  oy = fullscreen() ? 0 : (int16_t)ui::CONTENT_Y;
                    uint16_t ah = fullscreen() ? h : (uint16_t)(h - ui::CONTENT_Y);
                    ui::renderComponentFrame(root, c, st, ui::roleMetrics(), 0, oy, w, ah);
                }
            }
            ctx.present();
            dirty = false;
        }

        uint32_t tick = tickIntervalMs();
        bool gliding = st.dragScroll && st.dragScroll->velocity != 0.0f;
        uint32_t timeout = gliding ? 16 : (tick ? tick : 80);

        // Process one input event (updates dirty/st by reference).
        auto processEvent = [&](const InputEvent& ev) {
            // ── Touch path ────────────────────────────────────────────────
            if (ev.kind == InputEvent::Kind::Pointer) {
                input::PointerEvent pe{ev.pphase, ev.px, ev.py};
                if (onPointer(pe, ctx)) dirty = true;   // raw observer hook
                if (root && !capturesInput())
                    if (ui::dispatchPointer(root, st, pe)) dirty = true;
                return;
            }

            // ── Button path ───────────────────────────────────────────────
            if (ev.type != InputEvent::Type::Press && ev.type != InputEvent::Type::Repeat)
                return;
            st.modality = input::InputModality::Button;   // ring returns
            if (capturesInput()) {
                if (onKey(ev.key, ctx)) dirty = true;
            } else if (ev.key == Key::Cancel) {
                if (onKey(Key::Cancel, ctx)) dirty = true;
                else                         ctx.requestExit();
            } else if (root && navFromKey(ev.key, root, st)) {
                dirty = true;
            } else if (onKey(ev.key, ctx)) {
                dirty = true;
            }
        };

        InputEvent ev;
        if (ctx.waitInput(ev, timeout)) {
            // Drain the WHOLE pending burst, then render once. A fast drag
            // collapses to the latest finger position instead of replaying the
            // backlog one-frame-at-a-time (which looks smooth but lags ~1s+).
            processEvent(ev);
            while (ctx.nextInput(ev)) processEvent(ev);
        } else if (gliding) {
            if (ui::tickMomentum(st)) dirty = true;   // animate flick inertia
        } else if (tick) {
            // Periodic wake (no input) — rebuild only if state changed.
            if (onTick(ctx)) dirty = true;
        }
    }
}

} // namespace kairo
