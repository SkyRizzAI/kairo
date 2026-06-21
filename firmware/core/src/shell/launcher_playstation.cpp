// Plan 81 — PlayStation launcher skin (PS4/PS5-style horizontal strip).
//
// Layout (matches the reference): a small device title top-left, then a left→right
// strip of icon tiles. The focused tile is enlarged + filled, with a "Launch"
// action label beneath it and the focused item's NAME to its right; neighbours are
// small icon tiles. Position dots at the bottom. No big banner header.
#include "aether/shell/launcher_playstation.h"
#include "aether/shell/blit.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/ui_constants.h"

namespace nema::shell {

using namespace aether::ui;

// Draw a square tile with a centered, scaled icon. focused → filled (black) tile
// with a white icon; otherwise an outlined tile with a black icon. (drawPixel
// on=true paints black, so the icon colour is the OPPOSITE of `focused`.)
static void drawTile(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t side,
                     const LauncherEntry& e, bool focused) {
    aether::ui::draw::box_rounded(c, x, y, side, side, focused);
    if (e.icon && e.iconW && e.iconH && side > 6) {
        uint16_t isz = (uint16_t)(side * 3 / 5);
        uint16_t ix  = (uint16_t)(x + (side - isz) / 2);
        uint16_t iy  = (uint16_t)(y + (side - isz) / 2);
        blitScaledMask(c, e.icon, e.iconW, e.iconH, ix, iy, isz, isz, /*color=*/!focused);
    }
}

void PlayStationLauncher::draw(nema::Canvas& c, const LauncherModel& m, int cursor) {
    using namespace aether::ui::draw;
    const aether::StyleTokens& t = aether::theme();
    uint16_t W = c.width(), H = c.height();
    int n = m.count;
    if (n <= 0) return;
    if (cursor < 0) cursor = 0; else if (cursor >= n) cursor = n - 1;

    uint16_t top = nema::display::contentY();

    // Device title (small, Body) top-left.
    {
        FontSpec fs = fontForRole(TextRole::Body);
        c.setFont(fs.handle);
        const char* title = (m.title && *m.title) ? m.title : "PALANU";
        if (fs.scale <= 1) c.drawText(5, top, title, true);
        else               c.drawTextScaled(5, top, title, fs.scale, true);
    }
    uint16_t titleH  = measureTextH(TextRole::Body);
    uint16_t launchH = measureTextH(TextRole::Body);     // "Launch" scales with theme
    uint16_t nameH   = measureTextH(TextRole::Subhead);

    // Vertical band for the strip (between title and the bottom scrollbar).
    uint16_t posY    = (uint16_t)(H > t.space.lg ? H - t.space.lg : 0);
    uint16_t bandTop = (uint16_t)(top + titleH + t.space.xs);
    uint16_t bandBot = (uint16_t)(posY > t.space.sm ? posY - t.space.sm : posY);
    uint16_t bandH   = (bandBot > bandTop) ? (uint16_t)(bandBot - bandTop) : 0;

    // Tile size is derived from the (theme-scaled) text height so the tile, icon,
    // "Launch" and the app name all grow together when the theme is "large" — no
    // more tiny icon/Launch next to a doubled name. Clamped to stay on-screen.
    uint16_t big = (uint16_t)(nameH * 5);
    uint16_t maxBig = (uint16_t)(W / 2); if (maxBig > bandH) maxBig = bandH;
    uint16_t minBig = (uint16_t)(W / 5);
    if (big > maxBig) big = maxBig;
    if (big < minBig) big = minBig;
    uint16_t small = (uint16_t)(big * 3 / 5);
    uint16_t gap   = t.space.sm;

    // Vertically CENTER the strip in the band → responsive on any panel height.
    uint16_t bigTop = (uint16_t)(bandTop + (bandH > big ? (bandH - big) / 2 : 0));
    uint16_t chipH  = (uint16_t)(launchH + 2);                  // the "Launch" band height
    uint16_t chipY  = (uint16_t)(bigTop + big - chipH);         // bottom band of the big tile
    // Neighbours' centres align with the big tile's ICON centre (the area above the
    // band), so the big tile reads as "the same tile, just larger" with its Launch
    // band protruding below the small ones.
    uint16_t iconMidY = (uint16_t)(bigTop + (big - chipH) / 2);
    uint16_t smallTop = (uint16_t)(iconMidY > small / 2 ? iconMidY - small / 2 : 0);

    // Show a left-neighbour only when the screen is wide enough that we'd still
    // fit ≥2 right tiles after it. On tight logical resolutions (≤3 tiles total)
    // the selected tile sits flush-left — no "already-seen" buffer eating slots.
    bool showLeft = (cursor > 0) &&
                    ((int)(W - 5 - small - gap - big - gap) / (int)(small + gap) >= 2);

    uint16_t x = 5;

    if (showLeft) {
        drawTile(c, x, smallTop, small, m.items[cursor - 1], false);
        x = (uint16_t)(x + small + gap);
    }

    // Focused tile: SAME outline + (transparent) fill as the neighbours, only LARGER.
    uint16_t bigX = x;
    box_rounded(c, bigX, bigTop, big, big, /*filled=*/false);
    {
        const LauncherEntry& e = m.items[cursor];
        if (e.icon && e.iconW && e.iconH) {
            uint16_t areaH = (chipY > bigTop) ? (uint16_t)(chipY - bigTop) : big;
            uint16_t isz = (uint16_t)(big * 3 / 5);
            if (isz > areaH) isz = areaH;
            uint16_t ix = (uint16_t)(bigX + (big - isz) / 2);
            uint16_t iy = (uint16_t)(bigTop + (areaH > isz ? (areaH - isz) / 2 : 0));
            blitScaledMask(c, e.icon, e.iconW, e.iconH, ix, iy, isz, isz, /*white=*/true);
        }
    }
    // "Launch" band: filled in the BORDER colour (white), with inverted (black) text.
    {
        c.fillRect((uint16_t)(bigX + 1), chipY,
                   (uint16_t)(big > 2 ? big - 2 : big),
                   (uint16_t)(chipH > 1 ? chipH - 1 : chipH), /*on=*/true);
        FontSpec fs = fontForRole(TextRole::Body);
        c.setFont(fs.handle);
        const char* act = "Launch";
        uint16_t lw = measureTextW(act, TextRole::Body);
        uint16_t lx = (uint16_t)(bigX + (big > lw ? (big - lw) / 2 : 0));
        if (fs.scale <= 1) c.drawText(lx, (uint16_t)(chipY + 1), act, /*on=*/false);
        else               c.drawTextScaled(lx, (uint16_t)(chipY + 1), act, fs.scale, false);
    }

    // App name to the RIGHT of the big tile, bottom-aligned with the band (white).
    {
        FontSpec fs = fontForRole(TextRole::Subhead);
        c.setFont(fs.handle);
        const char* nm = m.items[cursor].label ? m.items[cursor].label : "";
        uint16_t nx = (uint16_t)(bigX + big + gap);
        uint16_t ny = (bigTop + big > nameH) ? (uint16_t)(bigTop + big - nameH) : bigTop;
        if (fs.scale <= 1) c.drawText(nx, ny, nm, true);
        else               c.drawTextScaled(nx, ny, nm, fs.scale, true);
    }

    // Following neighbours — start right after the big tile (the name sits lower, so
    // they don't collide), at the same small-tile row.
    x = (uint16_t)(bigX + big + gap);
    for (int i = cursor + 1; i < n; i++) {
        if (x + small > W) break;
        drawTile(c, x, smallTop, small, m.items[i], false);
        x = (uint16_t)(x + small + gap);
    }

    // Horizontal position bar — the same Flipper-style scrollbar as the Wii grid,
    // just horizontal along the bottom. Model the carousel as a 1-of-n window so the
    // thumb is ~1/n wide and slides with the cursor.
    scrollbar(c, 2, posY, (uint16_t)(W - 4), (uint16_t)cursor, 1, (uint16_t)n, /*horizontal=*/true);
}

} // namespace nema::shell
