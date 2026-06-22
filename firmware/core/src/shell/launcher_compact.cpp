// Compact / Coverflow launcher skin ("compact").
//
// A pennant carousel (Flipper-Zero Momentum "Compact"). The focused entry is a
// rounded SQUARE in the centre; its neighbours are EQUAL-HEIGHT banners whose
// INNER edge tapers to a chevron point aimed at the centre, like cards tucked
// behind one another (the tips converge on the middle):  > > | < <
//
//      ____    ____    ______________    ____    ____
//   \ |    \ |    \ |              | /    | /    | /
//    >| ic  >| ic  >|   CENTRE     |<  ic |<  ic |<     ← all the same height;
//   / |____/ |____/ |   square     | \____| \____|        the POINT faces centre
//                   |______________|                     (NOT a shrinking lens).
//
// App name is centred below the focused card; a horizontal position bar sits at
// the very bottom, matching the other skins.
#include "aether/shell/launcher_compact.h"
#include "aether/shell/blit.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/ui_constants.h"

namespace nema::shell {

using namespace aether::ui;

// Draw one side pennant. The rectangular body is [bodyX0, bodyX1] × [yt, yb]; the
// edge FACING the centre collapses to a chevron point at `tipX` on the centre line
// `cy`. The opposite (outer) edge stays a straight vertical. tipX outside the body
// on the left → a `<`-tip (right-side cards); on the right → a `>`-tip (left side).
static void drawPennant(nema::Canvas& c, uint16_t bodyX0, uint16_t bodyX1,
                        uint16_t yt, uint16_t yb, uint16_t cy, uint16_t tipX,
                        const LauncherEntry& e) {
    bool     tipLeft   = tipX <= bodyX0;
    uint16_t straightX = tipLeft ? bodyX1 : bodyX0;   // outer, away from centre
    uint16_t nearX     = tipLeft ? bodyX0 : bodyX1;   // body edge nearest the tip

    // Outer straight vertical edge.
    c.drawLine(straightX, yt, straightX, yb, true);
    // Flat top & bottom edges (straight edge → near-body edge).
    c.drawLine(straightX, yt, nearX, yt, true);
    c.drawLine(straightX, yb, nearX, yb, true);
    // Two slants converging on the inward-facing tip.
    c.drawLine(nearX, yt, tipX, cy, true);
    c.drawLine(nearX, yb, tipX, cy, true);

    // Icon — centred in the rectangular body, ~half the banner height.
    uint16_t bodyW = (uint16_t)(bodyX1 - bodyX0);
    uint16_t cardH = (uint16_t)(yb - yt);
    if (e.icon && e.iconW && e.iconH && bodyW > 5 && cardH > 6) {
        uint16_t isz = (uint16_t)((bodyW < cardH ? bodyW : cardH) * 11 / 20);
        if (isz < 2) isz = 2;
        uint16_t ix = (uint16_t)(bodyX0 + (bodyW > isz ? (bodyW - isz) / 2 : 0));
        uint16_t iy = (uint16_t)(cy - isz / 2);
        blitScaledMask(c, e.icon, e.iconW, e.iconH, ix, iy, isz, isz, true);
    }
}

void CompactLauncher::draw(nema::Canvas& c, const LauncherModel& m, int cursor) {
    uint16_t W = c.width(), H = c.height();
    int n = m.count;
    if (n <= 0) return;
    if (cursor < 0) cursor = 0; else if (cursor >= n) cursor = n - 1;

    const aether::StyleTokens& t = aether::theme();
    uint16_t top    = nema::display::contentY();        // below the status bar
    uint16_t sbH    = (uint16_t)t.space.sm;             // bottom scrollbar strip
    uint16_t labelH = measureTextH(TextRole::Subhead);  // app-name line

    uint16_t botY   = (uint16_t)(H - sbH - labelH - 2);
    uint16_t bandH  = (botY > top) ? (uint16_t)(botY - top) : 0;
    if (bandH < 8) return;

    // Centre card = SQUARE; all cards share this height (no lens taper).
    uint16_t s = bandH;
    uint16_t maxS = (uint16_t)(W * 2 / 5);
    if (s > maxS) s = maxS;

    uint16_t cy = (uint16_t)(top + bandH / 2);
    uint16_t cX = (uint16_t)((W - s) / 2);
    uint16_t cY = (uint16_t)(cy - s / 2);
    uint16_t yt = cY, yb = (uint16_t)(cY + s);

    uint16_t sw     = (uint16_t)(s * 50 / 100);         // side-banner body width
    uint16_t tipLen = (uint16_t)(sw * 45 / 100);        // chevron depth

    // ── right pennants: tips point LEFT (`<`), toward the centre ──────────────
    {
        uint16_t x = (uint16_t)(cX + s);                // tip boundary = centre edge
        for (int i = cursor + 1; i < n; i++) {
            uint16_t tipX   = x;
            uint16_t bodyX0 = (uint16_t)(x + tipLen);
            uint16_t bodyX1 = (uint16_t)(bodyX0 + sw);
            if (bodyX0 >= (uint16_t)(W - 2)) break;
            if (bodyX1 > (uint16_t)(W - 1)) bodyX1 = (uint16_t)(W - 1);
            if (bodyX1 - bodyX0 < 4) break;
            drawPennant(c, bodyX0, bodyX1, yt, yb, cy, tipX, m.items[i]);
            x = bodyX1;
        }
    }

    // ── left pennants: tips point RIGHT (`>`), toward the centre ──────────────
    {
        uint16_t x = cX;                                // tip boundary = centre edge
        for (int i = cursor - 1; i >= 0; i--) {
            if (x < (uint16_t)(tipLen + 5)) break;
            uint16_t tipX   = x;
            uint16_t bodyX1 = (uint16_t)(x - tipLen);
            uint16_t bodyX0 = (bodyX1 > sw) ? (uint16_t)(bodyX1 - sw) : 0;
            if (bodyX1 - bodyX0 < 4) break;
            drawPennant(c, bodyX0, bodyX1, yt, yb, cy, tipX, m.items[i]);
            x = bodyX0;
        }
    }

    // ── centre card (rounded square, painted last so it sits on top) ──────────
    c.fillRect(cX, cY, s, s, false);
    draw::box_rounded(c, cX, cY, s, s, false);
    {
        const LauncherEntry& e = m.items[cursor];
        if (e.icon && e.iconW && e.iconH && s > 6) {
            uint16_t isz = (uint16_t)(s * 3 / 5);
            uint16_t ix  = (uint16_t)(cX + (s - isz) / 2);
            uint16_t iy  = (uint16_t)(cY + (s - isz) / 2);
            blitScaledMask(c, e.icon, e.iconW, e.iconH, ix, iy, isz, isz, true);
        }
    }

    // ── app name (centred below the focused card) ─────────────────────────────
    {
        FontSpec    fs = fontForRole(TextRole::Subhead);
        c.setFont(fs.handle);
        const char* nm = m.items[cursor].label ? m.items[cursor].label : "";
        uint16_t    lw = measureTextW(nm, TextRole::Subhead);
        uint16_t    lx = (uint16_t)(W > lw ? (W - lw) / 2 : 0);
        uint16_t    ly = (uint16_t)(botY + 1);
        if (fs.scale <= 1) c.drawText(lx, ly, nm, true);
        else               c.drawTextScaled(lx, ly, nm, fs.scale, true);
    }

    // ── position scrollbar ───────────────────────────────────────────────────
    draw::scrollbar(c, 2, (uint16_t)(H - sbH), (uint16_t)(W - 4),
                    (uint16_t)cursor, 1, (uint16_t)n, true);
}

} // namespace nema::shell
