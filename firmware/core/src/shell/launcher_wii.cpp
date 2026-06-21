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
#include <cstring>

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

// Compute a virtual row index for each item, inserting a section-header "row" before
// the first item of each section that differs from the previous item's section.
// Returns the total virtual row count and fills rowMap[i] = virtual row for item i,
// and sectionRow[i] = virtual row of the header if item i starts a new section, else -1.
static int buildRowMap(const LauncherModel& m, int cols,
                       int* rowMap, int* sectionRow) {
    int vrow = 0, col = 0;
    const char* lastSection = nullptr;
    for (int i = 0; i < m.count; i++) {
        const char* sec = m.items[i].section ? m.items[i].section : "Apps";
        bool newSec = (lastSection == nullptr ||
                       (lastSection != sec && std::strcmp(lastSection, sec) != 0));
        if (newSec) {
            if (col != 0) { vrow++; col = 0; }  // close incomplete row first
            sectionRow[i] = vrow++;              // header row
            lastSection = sec;
        } else {
            sectionRow[i] = -1;
        }
        rowMap[i] = vrow;
        if (++col >= cols) { col = 0; vrow++; }
    }
    if (col != 0) vrow++;
    return vrow;
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
    uint8_t  sbW = 4;
    uint16_t top = nema::display::contentY();
    uint16_t botPad = t.space.lg;
    uint16_t areaH = (H > top + botPad) ? (uint16_t)(H - top - botPad) : 0;

    uint16_t gridW = (W > sbW) ? (uint16_t)(W - sbW) : W;
    uint16_t tileW = (uint16_t)((gridW - gap * (cols + 1)) / cols);
    uint16_t tileH = (uint16_t)(tileW * 3 / 4);
    if (areaH > (uint16_t)(gap * 3)) {
        uint16_t maxH2 = (uint16_t)((areaH - gap * 3) / 2);
        if (tileH > maxH2) tileH = maxH2;
    }

    uint16_t secHdrH = (uint16_t)(measureTextH(TextRole::Caption) + (uint16_t)gap);
    uint16_t rowH    = (uint16_t)(tileH + gap);

    // Build virtual-row map (section header rows + tile rows).
    int rowMap[64] = {}, sectionRow[64] = {};
    int nItems = n < 64 ? n : 64;
    int totalVRows = buildRowMap(m, cols, rowMap, sectionRow);

    // Visible virtual row count: approximate using rowH (close enough).
    int visVRows = rowH ? (int)(areaH / rowH) : 1;
    if (visVRows < 1) visVRows = 1;
    int curVRow  = rowMap[cursor < nItems ? cursor : nItems - 1];
    int firstVRow = 0;
    if (curVRow >= visVRows) firstVRow = curVRow - visVRows + 1;

    // Per-virtual-row y offsets, computed as we scan.
    // Row y depends on whether the row is a section header or tile row.
    // We compute offsets on the fly to keep it simple (at most ~8 vrows).
    auto vrY = [&](int vr) -> int {
        // Walk from 0 to vr, counting header rows vs tile rows.
        int y = (int)top;
        for (int v = 0; v < vr; v++) {
            bool isHdr = false;
            for (int i = 0; i < nItems; i++) {
                if (sectionRow[i] == v) { isHdr = true; break; }
            }
            y += isHdr ? (int)secHdrH : (int)rowH;
        }
        return y;
    };

    // Draw section headers and tiles.
    for (int i = 0; i < nItems; i++) {
        int secVR = sectionRow[i];
        if (secVR >= 0) {
            int vr = secVR - firstVRow;
            if (vr >= 0 && vr < visVRows + 2) {
                int y = vrY(secVR - firstVRow + firstVRow) - vrY(firstVRow);
                int ay = (int)top + y;
                if (ay >= 0 && ay < (int)H) {
                    const char* sec = m.items[i].section ? m.items[i].section : "Apps";
                    FontSpec fs = fontForRole(TextRole::Caption);
                    c.setFont(fs.handle);
                    c.drawText(gap, (uint16_t)ay, sec, true);
                    c.drawLine(gap, (uint16_t)(ay + secHdrH - 2),
                               (uint16_t)(gridW - gap), (uint16_t)(ay + secHdrH - 2), true);
                }
            }
        }

        int tileVR  = rowMap[i];
        int col     = 0;
        // compute col for this item
        {
            int cnt = 0;
            for (int j = 0; j < i; j++) {
                if (rowMap[j] == tileVR) cnt++;
            }
            col = cnt;
        }

        int dy = vrY(tileVR) - vrY(firstVRow);
        int ay = (int)top + dy;
        if (ay < -(int)tileH || ay >= (int)H) continue;

        uint16_t tx = (uint16_t)(gap + col * (tileW + gap));
        drawChannel(c, tx, (uint16_t)ay, tileW, tileH, m.items[i], i == cursor);
    }

    if (totalVRows > visVRows)
        scrollbar(c, (uint16_t)(W - sbW + 1), top, areaH,
                  (uint16_t)(firstVRow * rowH), (uint16_t)(visVRows * rowH),
                  (uint16_t)(totalVRows * rowH), false);
}

} // namespace nema::shell
