#include "nema/ui/components.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"

namespace nema {
namespace ui {

uint16_t drawTitle(Canvas& c, const char* title) {
    // 4px padding between the status-bar separator and the title text so the
    // header never sits cramped against the top line.
    uint16_t ty = CONTENT_Y + 4;
    c.drawText(c.centerX(title), ty, title);
    uint16_t sepY = ty + CHAR_H + 3;       // separator under the title
    c.fillRect(0, sepY, c.width(), 1);
    return sepY + 5;                        // content starts below, with padding
}

uint16_t modalOriginX(const Canvas& c, uint16_t w) {
    return (c.width()  > w) ? (uint16_t)((c.width()  - w) / 2) : 0;
}
uint16_t modalOriginY(const Canvas& c, uint16_t h) {
    return (c.height() > h) ? (uint16_t)((c.height() - h) / 2) : 0;
}

void drawModalBox(Canvas& c, uint16_t w, uint16_t h) {
    uint16_t mx = modalOriginX(c, w);
    uint16_t my = modalOriginY(c, h);
    c.fillRect(mx, my, w, h, false);  // clear interior (white)
    c.drawRect(mx, my, w, h, true);   // border
}

void drawConfirm(Canvas& c, const char* prompt, int cursor,
                 uint16_t w, uint16_t h) {
    drawModalBox(c, w, h);
    uint16_t mx = modalOriginX(c, w);
    uint16_t my = modalOriginY(c, h);

    c.drawText(mx + 8, my + 10, prompt);

    uint16_t by = my + h - CHAR_H - 8;

    const char* yes = "Yes";
    uint16_t yw = c.textWidth(yes);
    uint16_t yx = mx + w / 4 - yw / 2;
    if (cursor == 0) {
        c.invertRect(yx - 4, by - 2, yw + 8, CHAR_H + 4);
        c.drawText(yx, by, yes, false);
    } else {
        c.drawText(yx, by, yes);
    }

    const char* no = "No";
    uint16_t nw = c.textWidth(no);
    uint16_t nx = mx + 3 * w / 4 - nw / 2;
    if (cursor == 1) {
        c.invertRect(nx - 4, by - 2, nw + 8, CHAR_H + 4);
        c.drawText(nx, by, no, false);
    } else {
        c.drawText(nx, by, no);
    }
}

} // namespace ui
} // namespace nema
