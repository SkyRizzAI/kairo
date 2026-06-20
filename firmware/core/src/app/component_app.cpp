#include "nema/app/component_app.h"
#include "nema/app/app_context.h"
#include "nema/ui/canvas.h"
#include "nema/ui/component_runtime.h"
#include "nema/ui/text_style.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/aether_abi.h"
#include "nema/input_event.h"

namespace nema {

// Map a physical Key to component navigation; returns true if it moved focus or
// activated. Up/Left = Prev, Down/Right = Next, Select = Activate.
static bool navFromKey(Key k, aether::ui::UiNode* root, aether::ui::ComponentState& st) {
    switch (k) {
        case Key::Up:     return aether::ui::dispatchNav(root, st, aether::ui::Nav::Prev);
        case Key::Down:   return aether::ui::dispatchNav(root, st, aether::ui::Nav::Next);
        case Key::Select: return aether::ui::dispatchNav(root, st, aether::ui::Nav::Activate);
        // Left/Right: adjust a focused value control first, else move focus.
        case Key::Left:   return aether::ui::dispatchAdjust(root, st, -1) || aether::ui::dispatchNav(root, st, aether::ui::Nav::Prev);
        case Key::Right:  return aether::ui::dispatchAdjust(root, st, +1) || aether::ui::dispatchNav(root, st, aether::ui::Nav::Next);
        default:          return false;
    }
}

void ComponentApp::run(AppContext& ctx) {
    aether::ui::NodeArena arena(arenaCapacity());
    aether::ui::UiNode* root  = nullptr;
    aether::ui::UiNode* modal = nullptr;
    bool dirty = true;

    // Shared interaction state (Plan 30 runtime): focus + dual modality +
    // tap/drag/momentum. A SEPARATE state drives the modal layer so the base's
    // focus/scroll is frozen and untouched while a modal is up.
    aether::ui::ComponentState st;
    aether::ui::ComponentState modalSt;

    onStart(ctx);

    while (!ctx.shouldExit()) {
        if (dirty) {
            Canvas& c = ctx.canvas();
            c.clear();
            if (drawRaw(c, ctx)) {
                root = nullptr; modal = nullptr;   // custom-drawn frame (e.g. keyboard)
            } else {
                arena.reset();
                st.pressed = nullptr;        // arena reset invalidates node pointers
                modalSt.pressed = nullptr;
                aether_set_arena(&arena);    // Plan 50: expose arena to ABI functions
                root  = build(arena, ctx);
                modal = buildModal(arena, ctx);
                aether_set_arena(nullptr);

                uint16_t w = c.width();
                uint16_t h = c.height();
                int16_t  oy = fullscreen() ? 0 : (int16_t)nema::display::CONTENT_Y;
                uint16_t ah = fullscreen() ? h : (uint16_t)(h - nema::display::CONTENT_Y);

                if (root) {
                    // Base frame. When a modal is up, suppress the base focus ring
                    // (focus belongs to the modal) by rendering it pointer-modality.
                    input::InputModality saved = st.modality;
                    if (modal) st.modality = input::InputModality::Pointer;
                    aether::ui::renderComponentFrame(root, c, st, aether::ui::roleMetrics(), 0, oy, w, ah);
                    if (modal) st.modality = saved;
                }

                if (modal) {
                    uint16_t mw = modal->style.width  != aether::ui::SIZE_AUTO ? modal->style.width  : (uint16_t)(w * 3 / 4);
                    uint16_t mh = modal->style.height != aether::ui::SIZE_AUTO ? modal->style.height : (uint16_t)(h / 2);
                    if (mw > w) mw = w;
                    if (mh > h) mh = h;
                    int16_t mx = (int16_t)((w - mw) / 2);
                    int16_t my = (int16_t)((h - mh) / 2);
                    // White backdrop + border so the modal masks the base behind it.
                    c.fillRect(mx, my, mw, mh, false);
                    c.drawRect(mx, my, mw, mh, true);
                    aether::ui::renderComponentFrame(modal, c, modalSt, aether::ui::roleMetrics(), mx, my, mw, mh);
                }
            }
            ctx.present();
            dirty = false;
        }

        // While a modal is up it captures all interaction.
        aether::ui::UiNode*         active = modal ? modal    : root;
        aether::ui::ComponentState& ast    = modal ? modalSt  : st;

        uint32_t tick = tickIntervalMs();
        bool gliding = ast.dragScroll && ast.dragScroll->velocity != 0.0f;
        uint32_t timeout = gliding ? 16 : (tick ? tick : 80);

        // Process one input event (updates dirty/ast by reference).
        auto processEvent = [&](const InputEvent& ev) {
            // ── Touch path ────────────────────────────────────────────────
            if (ev.kind == InputEvent::Kind::Pointer) {
                input::PointerEvent pe{ev.pphase, ev.px, ev.py};
                if (onPointer(pe, ctx)) dirty = true;   // raw observer hook
                if (active && !capturesInput())
                    if (aether::ui::dispatchPointer(active, ast, pe)) dirty = true;
                return;
            }

            // ── Button path ───────────────────────────────────────────────
            if (ev.type != InputEvent::Type::Press && ev.type != InputEvent::Type::Repeat)
                return;
            ast.modality = input::InputModality::Button;   // ring returns
            if (capturesInput()) {
                // Raw apps receive every key. Honor the onKey() contract: a false
                // return on Cancel exits the app — without this, a capturesInput
                // app that defers Cancel can never be exited (it gets stuck).
                if (onKey(ev.key, ctx)) dirty = true;
                else if (ev.key == Key::Cancel && !modal) ctx.requestExit();
            } else if (ev.key == Key::Cancel) {
                if (onKey(Key::Cancel, ctx)) dirty = true;
                else if (!modal)             ctx.requestExit();   // modal up: don't exit app
            } else if (active && navFromKey(ev.key, active, ast)) {
                dirty = true;
            } else if (onKey(ev.key, ctx)) {
                dirty = true;
            }
        };

        InputEvent ev;
        if (ctx.waitInput(ev, timeout)) {
            // Drain the WHOLE pending burst, then render once.
            processEvent(ev);
            while (ctx.nextInput(ev)) processEvent(ev);
        } else if (gliding) {
            if (aether::ui::tickMomentum(ast)) dirty = true;   // animate flick inertia
        } else if (tick) {
            // Periodic wake (no input) — rebuild only if state changed.
            if (onTick(ctx)) dirty = true;
        }
    }
}

} // namespace nema
