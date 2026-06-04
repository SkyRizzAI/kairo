#include "kairo/apps/touch_test_app.h"
#include "kairo/app/app_context.h"
#include "kairo/ui/canvas.h"
#include <cstdio>

namespace kairo {

ui::UiNode* TouchTestApp::build(ui::NodeArena&, AppContext&) {
    return nullptr;   // drawRaw() owns the frame
}

bool TouchTestApp::onPointer(const input::PointerEvent& e, AppContext&) {
    x_ = (int)e.x;
    y_ = (int)e.y;
    if (e.phase == input::PointerPhase::Down) { down_ = true;  taps_++; }
    if (e.phase == input::PointerPhase::Up)   { down_ = false; }
    return true;   // redraw every event
}

bool TouchTestApp::drawRaw(Canvas& c, AppContext&) {
    const uint16_t W = c.width();
    const uint16_t H = c.height();
    c.clear(false);

    // Crosshair that follows the live touch point (Down + drag Move + Up).
    if (x_ >= 0 && y_ >= 0) {
        c.drawLine(0, (uint16_t)y_, W - 1, (uint16_t)y_, true);
        c.drawLine((uint16_t)x_, 0, (uint16_t)x_, H - 1, true);
        c.fillRect((uint16_t)(x_ > 3 ? x_ - 3 : 0),
                   (uint16_t)(y_ > 3 ? y_ - 3 : 0), 7, 7, true);
    }

    // Header + live readout
    c.drawText(4, 3, "TOUCH TEST", true);

    char line[40];
    std::snprintf(line, sizeof(line), "x=%-4d y=%-4d %s",
                  x_, y_, down_ ? "DOWN" : "up  ");
    c.drawText(4, 14, line, true);

    std::snprintf(line, sizeof(line), "taps=%u  res=%dx%d",
                  (unsigned)taps_, (int)W, (int)H);
    c.drawText(4, 25, line, true);

    // Corner markers so orientation is unmistakable
    c.drawText(2, 36, "TL", true);
    c.drawText(W - 18, 36, "TR", true);
    c.drawText(2, H - 11, "BL", true);
    c.drawText(W - 18, H - 11, "BR", true);

    c.drawText(4, H - 22, "Hold OK = exit", true);
    return true;   // we painted the whole frame
}

} // namespace kairo
