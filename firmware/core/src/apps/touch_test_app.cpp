#include "nema/apps/touch_test_app.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/ui/canvas.h"
#include <cstdio>

namespace nema {

aether::ui::UiNode* TouchTestApp::build(aether::ui::NodeArena&, AppContext&) {
    return nullptr;   // drawRaw() owns the frame
}

bool TouchTestApp::onPointer(const input::PointerEvent& e, AppContext&) {
    x_ = (int)e.x;
    y_ = (int)e.y;
    if (e.phase == input::PointerPhase::Down) { down_ = true;  taps_++; }
    if (e.phase == input::PointerPhase::Up)   { down_ = false; }
    return true;   // redraw every event
}

bool TouchTestApp::drawRaw(Canvas& c, AppContext& ctx) {
    const uint16_t W = c.width();
    const uint16_t H = c.height();
    c.clear(false);

    // Small marker that follows the live touch point (box outline + center dot).
    // Kept SMALL so the partial-flush row-diff only re-sends the few rows around
    // it → minimal latency.
    if (x_ >= 0 && y_ >= 0) {
        const int S = 8;
        int bx = x_ - S; if (bx < 0) bx = 0;
        int by = y_ - S; if (by < 0) by = 0;
        int bw = 2 * S + 1; if (bx + bw > (int)W) bw = (int)W - bx;
        int bh = 2 * S + 1; if (by + bh > (int)H) bh = (int)H - by;
        c.drawRect((uint16_t)bx, (uint16_t)by, (uint16_t)bw, (uint16_t)bh, true);
        c.fillRect((uint16_t)(x_ > 2 ? x_ - 2 : 0),
                   (uint16_t)(y_ > 2 ? y_ - 2 : 0), 5, 5, true);
    }

    // Header + live readout
    c.drawText(4, 3, "TOUCH TEST", true);

    char line[40];
    std::snprintf(line, sizeof(line), "x=%-4d y=%-4d %s",
                  x_, y_, down_ ? "DOWN" : "up  ");
    c.drawText(4, 14, line, true);

    std::snprintf(line, sizeof(line), "taps=%u  fps=%u",
                  (unsigned)taps_, (unsigned)ctx.runtime().fps());
    c.drawText(4, 25, line, true);

    // Corner markers so orientation is unmistakable
    c.drawText(2, 36, "TL", true);
    c.drawText(W - 18, 36, "TR", true);
    c.drawText(2, H - 11, "BL", true);
    c.drawText(W - 18, H - 11, "BR", true);
    return true;   // we painted the whole frame (Back exits — handled by the input HAL)
}

} // namespace nema
