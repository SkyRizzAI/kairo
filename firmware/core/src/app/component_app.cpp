#include "kairo/app/component_app.h"
#include "kairo/app/app_context.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/layout.h"
#include "kairo/ui/renderer.h"
#include "kairo/ui/focus.h"
#include "kairo/ui/text_style.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/nema/input_event.h"

namespace kairo {

void ComponentApp::run(AppContext& ctx) {
    ui::NodeArena arena(arenaCapacity());
    ui::FocusState fs;
    ui::UiNode* root = nullptr;
    bool dirty = true;

    onStart(ctx);

    while (!ctx.shouldExit()) {
        if (dirty) {
            Canvas& c = ctx.canvas();
            c.clear();
            if (drawRaw(c, ctx)) {
                root = nullptr;   // custom-drawn frame (e.g. keyboard)
            } else {
                arena.reset();
                root = build(arena, ctx);
                if (root) {
                    uint16_t w = c.width();
                    uint16_t h = c.height();
                    // Normal mode leaves the top strip for the system status bar.
                    int16_t  oy = fullscreen() ? 0 : (int16_t)ui::CONTENT_Y;
                    uint16_t ah = fullscreen() ? h : (uint16_t)(h - ui::CONTENT_Y);
                    ui::layout(*root, w, ah, ui::roleMetrics(), 0, oy);
                    ui::UiNode* foc = ui::focusedNode(*root, fs);
                    ui::render(*root, c, foc);
                }
            }
            ctx.present();
            dirty = false;
        }

        uint32_t tick = tickIntervalMs();
        uint32_t timeout = tick ? tick : 80;

        InputEvent ev;
        if (ctx.waitInput(ev, timeout)) {
            if (ev.type != InputEvent::Type::Press && ev.type != InputEvent::Type::Repeat)
                continue;
            if (capturesInput()) {
                // Raw mode: every key (incl. arrows/Select/Cancel) goes to onKey.
                if (onKey(ev.key, ctx)) dirty = true;
            } else if (ev.key == Key::Cancel) {
                if (onKey(Key::Cancel, ctx)) dirty = true;
                else                         ctx.requestExit();
            } else if (root && ui::handleFocusKey(*root, fs, ev.key)) {
                dirty = true;   // focus moved or onPress fired
            } else if (onKey(ev.key, ctx)) {
                dirty = true;
            }
        } else if (tick) {
            // Periodic wake (no input) — rebuild only if state changed.
            if (onTick(ctx)) dirty = true;
        }
    }
}

} // namespace kairo
