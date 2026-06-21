// Plan 81 — Nintendo Wii launcher skin (channel grid).
//
// 2-column grid of "channel" tiles, each a rounded box with a 1px drop shadow
// (offset +1,+1) and a centered, scaled-up icon over a label. The focused channel
// is inverted (filled). A vertical scrollbar appears when the grid overflows.
#include "aether/shell/launcher_wii.h"
#include "aether/shell/blit.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/ui_constants.h"

namespace nema::shell {

using namespace aether::ui;

// A channel tile with a drop shadow. selected → filled (black) face + white
// content; otherwise white face + thin outline + black content.
static void drawChannel(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        const LauncherEntry& e, bool sel) {
    using namespace aether::ui::draw;
    box_rounded(c, (uint16_t)(x + 1), (uint16_t)(y + 1), w, h, /*filled=*/true);  // shadow
    if (sel) {
        box_rounded(c, x, y, w, h, /*filled=*/true);     // inverted face
    } else {
        c.fillRect(x, y, w, h, /*on=*/false);            // white face (covers shadow)
        box_rounded(c, x, y, w, h, /*filled=*/false);    // outline
    }

    uint16_t lblH = measureTextH(TextRole::Caption);
    // Icon — scaled up to fill the upper band (no longer a tiny 8×8).
    if (e.icon && e.iconW && e.iconH) {
        uint16_t bandH = (h > (uint16_t)(lblH + 3)) ? (uint16_t)(h - lblH - 3) : h;
        uint16_t isz = (uint16_t)(bandH * 4 / 5);
        uint16_t cap = (uint16_t)(w * 3 / 4);
        if (isz > cap) isz = cap;
        uint16_t ix = (uint16_t)(x + (w > isz ? (w - isz) / 2 : 0));
        uint16_t iy = (uint16_t)(y + (bandH > isz ? (bandH - isz) / 2 : 1));
        blitScaledMask(c, e.icon, e.iconW, e.iconH, ix, iy, isz, isz, /*color=*/!sel);
    }
    // Label centered along the bottom (white on a selected face, else black).
    if (e.label) {
        FontSpec fs = fontForRole(TextRole::Caption);
        c.setFont(fs.handle);
        uint16_t lw = measureTextW(e.label, TextRole::Caption);
        uint16_t lx = (uint16_t)(x + (w > lw ? (w - lw) / 2 : 0));
        uint16_t ly = (uint16_t)(y + h - lblH - 1);
        c.drawText(lx, ly, e.label, /*on=*/!sel);
    }
}

void WiiLauncher::draw(nema::Canvas& c, const LauncherModel& m, int cursor) {
    using namespace aether::ui::draw;
    const aether::StyleTokens& t = aether::theme();
    uint16_t W = c.width(), H = c.height();
    int n = m.count;
    if (n <= 0) return;
    if (cursor < 0) cursor = 0; else if (cursor >= n) cursor = n - 1;

    const int cols = 2;
    uint8_t  gap = t.space.sm;
    uint8_t  sbW = 4;                                   // scrollbar gutter
    uint16_t top = nema::display::contentY();
    uint16_t botPad = t.space.lg;
    uint16_t areaH = (H > top + botPad) ? (uint16_t)(H - top - botPad) : 0;

    uint16_t gridW = (W > sbW) ? (uint16_t)(W - sbW) : W;
    uint16_t tileW = (uint16_t)((gridW - gap * (cols + 1)) / cols);
    uint16_t tileH = (uint16_t)(tileW * 3 / 4);
    // 2-column tiles at 3:4 are too tall (only 1 row fits a 264×176 panel); cap the
    // height so at least 2 rows are visible, like the Wii channel grid.
    if (areaH > (uint16_t)(gap * 3)) {
        uint16_t maxH2 = (uint16_t)((areaH - gap * 3) / 2);
        if (tileH > maxH2) tileH = maxH2;
    }
    uint16_t rowH  = (uint16_t)(tileH + gap);

    int rows    = (n + cols - 1) / cols;
    int curRow  = cursor / cols;
    int visRows = rowH ? (areaH / rowH) : 1;
    if (visRows < 1) visRows = 1;
    int firstRow = 0;
    if (curRow >= visRows) firstRow = curRow - visRows + 1;

    for (int i = 0; i < n; i++) {
        int r = i / cols, col = i % cols;
        int vr = r - firstRow;
        if (vr < 0 || vr >= visRows) continue;
        uint16_t tx = (uint16_t)(gap + col * (tileW + gap));
        uint16_t ty = (uint16_t)(top + vr * rowH);
        drawChannel(c, tx, ty, tileW, tileH, m.items[i], i == cursor);
    }

    if (rows > visRows)
        scrollbar(c, (uint16_t)(W - sbW + 1), top, areaH,
                  (uint16_t)(firstRow * rowH), (uint16_t)(visRows * rowH),
                  (uint16_t)(rows * rowH), false);
}

} // namespace nema::shell
