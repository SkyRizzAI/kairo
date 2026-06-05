#include "kairo/app/component_app.h"
#include "kairo/app/app_context.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/layout.h"
#include "kairo/ui/renderer.h"
#include "kairo/ui/focus.h"
#include "kairo/ui/hit_test.h"
#include "kairo/ui/text_style.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/nema/input_event.h"

namespace kairo {

void ComponentApp::run(AppContext& ctx) {
    ui::NodeArena arena(arenaCapacity());
    ui::FocusState fs;
    ui::UiNode* root = nullptr;
    bool dirty = true;

    // Dual-modal state (Plan 29): focus-ring shows only in Button mode; a touch
    // flips to Pointer mode (ring hidden). `pressed` tracks the node hit on
    // Down so a release over the same node fires its onPress (like web click).
    input::InputModality modality = input::InputModality::Button;
    ui::UiNode*          pressed  = nullptr;

    onStart(ctx);

    while (!ctx.shouldExit()) {
        if (dirty) {
            Canvas& c = ctx.canvas();
            c.clear();
            if (drawRaw(c, ctx)) {
                root = nullptr;   // custom-drawn frame (e.g. keyboard)
            } else {
                arena.reset();
                pressed = nullptr;   // old node pointers are invalid after reset
                root = build(arena, ctx);
                if (root) {
                    uint16_t w = c.width();
                    uint16_t h = c.height();
                    // Normal mode leaves the top strip for the system status bar.
                    int16_t  oy = fullscreen() ? 0 : (int16_t)ui::CONTENT_Y;
                    uint16_t ah = fullscreen() ? h : (uint16_t)(h - ui::CONTENT_Y);
                    ui::layout(*root, w, ah, ui::roleMetrics(), 0, oy);
                    ui::UiNode* foc = ui::focusedNode(*root, fs);
                    // Focus ring only in Button mode (web :focus-visible).
                    ui::render(*root, c,
                               modality == input::InputModality::Button ? foc : nullptr);
                }
            }
            ctx.present();
            dirty = false;
        }

        uint32_t tick = tickIntervalMs();
        uint32_t timeout = tick ? tick : 80;

        // Process one input event (updates dirty/modality/pressed by reference).
        auto processEvent = [&](const InputEvent& ev) {
            // ── Touch path ────────────────────────────────────────────────
            if (ev.kind == InputEvent::Kind::Pointer) {
                modality = input::InputModality::Pointer;
                input::PointerEvent pe{ev.pphase, ev.px, ev.py};
                if (onPointer(pe, ctx)) dirty = true;   // raw observer hook
                if (root && !capturesInput()) {
                    if (ev.pphase == input::PointerPhase::Down) {
                        pressed = ui::hitTest(*root, (int16_t)ev.px, (int16_t)ev.py);
                    } else if (ev.pphase == input::PointerPhase::Up) {
                        ui::UiNode* up = ui::hitTest(*root, (int16_t)ev.px, (int16_t)ev.py);
                        if (up && up == pressed && up->onPress) {
                            up->onPress(up->userdata);   // web "click": down+up same node
                            dirty = true;
                        }
                        pressed = nullptr;
                    }
                }
                return;
            }

            // ── Button path ───────────────────────────────────────────────
            if (ev.type != InputEvent::Type::Press && ev.type != InputEvent::Type::Repeat)
                return;
            modality = input::InputModality::Button;   // ring returns
            if (capturesInput()) {
                if (onKey(ev.key, ctx)) dirty = true;
            } else if (ev.key == Key::Cancel) {
                if (onKey(Key::Cancel, ctx)) dirty = true;
                else                         ctx.requestExit();
            } else if (root && ui::handleFocusKey(*root, fs, ev.key)) {
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
        } else if (tick) {
            // Periodic wake (no input) — rebuild only if state changed.
            if (onTick(ctx)) dirty = true;
        }
    }
}

} // namespace kairo
