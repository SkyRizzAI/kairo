// Compact / 3D-card-stack launcher skin ("compact").
//
// The focused card is a flat square in the centre.  Its neighbours are drawn as
// the SIDE FACE of a 3D card tucked behind the centre — an isometric hexagon with
// a straight outer edge (full height) and a shorter inner edge trimmed by a skew
// amount, joined by two diagonal slants:
//
//     +---     +------------------+     ---+
//     |   \    |                  |    /   |
//     |    \   |                  |   /    |
//     |     |  |    CENTRE CARD   |  |     |
//     |     |  |                  |  |     |
//     |    /   |                  |   \    |
//     |   /    |                  |    \   |
//     +---     +------------------+     ---+
//
// Icons on side cards are drawn with a per-column vertical shear so they appear
// painted on the tilted face.  App name is centred below the focused card;
// a horizontal position bar sits at the very bottom.
#include "aether/shell/launcher_compact.h"
#include "aether/shell/blit.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/ui_constants.h"

namespace nema::shell {

using namespace aether::ui;

// 1-bit row-major bitmap helper (shared bit layout with blitScaledMask).
static inline bool srcBit(const uint8_t* bits, int sw, int sx, int sy) {
    uint32_t idx = (uint32_t)sy * sw + sx;
    return (bits[idx / 8] >> (7 - idx % 8)) & 1;
}

// Draw the isometric side-face of one card.
//
//   outerX  — x of the outer (away-from-centre) vertical edge (full height yt..yb)
//   innerX  — x of the inner (near-centre) vertical edge (shorter: (yt+skew)..(yb-skew))
//   leftCard — true when the card sits to the LEFT of the centre (outerX < innerX)
//
// The icon is rendered with a column-wise vertical shear that matches the face tilt:
//   left card  → columns shift DOWN as x increases toward centre
//   right card → columns shift DOWN as x decreases toward centre
static void drawSideCard(nema::Canvas& c,
                         uint16_t outerX, uint16_t innerX,
                         uint16_t yt, uint16_t yb,
                         uint16_t skew, bool leftCard,
                         const LauncherEntry& e) {
    uint16_t innerYt = (uint16_t)(yt + skew);
    uint16_t innerYb = (uint16_t)(yb > skew ? yb - skew : yt + 2);
    if (innerYb <= innerYt) return;

    // Hexagon outline
    c.drawLine(outerX, yt,      outerX, yb,      true);   // outer full-height edge
    c.drawLine(innerX, innerYt, innerX, innerYb,  true);   // inner shorter edge
    c.drawLine(outerX, yt,      innerX, innerYt,  true);   // top slant
    c.drawLine(outerX, yb,      innerX, innerYb,  true);   // bottom slant

    // Icon centred in the face, with per-column vertical shear
    uint16_t faceLeft  = leftCard ? outerX : innerX;
    uint16_t faceRight = leftCard ? innerX : outerX;
    uint16_t faceW     = (uint16_t)(faceRight - faceLeft);
    uint16_t innerH    = (uint16_t)(innerYb - innerYt);

    if (!e.icon || !e.iconW || !e.iconH || faceW < 5 || innerH < 5) return;

    uint16_t isz = (uint16_t)((faceW < innerH ? faceW : innerH) * 3 / 5);
    if (isz < 2) return;

    uint16_t iconX = (uint16_t)(faceLeft + (faceW - isz) / 2);
    uint16_t midY  = (uint16_t)((innerYt + innerYb) / 2);
    uint16_t iconY = (uint16_t)(midY > isz / 2 ? midY - isz / 2 : 0);

    for (uint16_t dx = 0; dx < isz; dx++) {
        int sx = (int)(dx * e.iconW / isz);
        // Shear amount for this column — matches the face's diagonal taper.
        int16_t shear = leftCard
            ? (int16_t)((int32_t)dx * skew / isz)          // left card: rightward → down
            : (int16_t)((int32_t)(isz - 1 - dx) * skew / isz); // right card: leftward → down

        for (uint16_t dy = 0; dy < isz; dy++) {
            int sy = (int)(dy * e.iconH / isz);
            if (srcBit(e.icon, e.iconW, sx, sy)) {
                c.drawPixel((uint16_t)(iconX + dx),
                            (uint16_t)(iconY + dy + (uint16_t)shear),
                            true);
            }
        }
    }
}

void CompactLauncher::draw(nema::Canvas& c, const LauncherModel& m, int cursor) {
    uint16_t W = c.width(), H = c.height();
    int n = m.count;
    if (n <= 0) return;
    if (cursor < 0) cursor = 0; else if (cursor >= n) cursor = n - 1;

    const aether::StyleTokens& t = aether::theme();
    uint16_t top    = nema::display::contentY();        // below status bar
    uint16_t sbH    = (uint16_t)t.space.sm;             // bottom scrollbar strip
    uint16_t labelH = measureTextH(TextRole::Subhead);  // app-name label

    uint16_t botY  = (uint16_t)(H - sbH - labelH - 2);
    uint16_t bandH = (botY > top) ? (uint16_t)(botY - top) : 0;
    if (bandH < 8) return;

    // Centre card: square, capped at 2/5 of display width.
    uint16_t s    = bandH;
    uint16_t maxS = (uint16_t)(W * 2 / 5);
    if (s > maxS) s = maxS;

    uint16_t cX = (uint16_t)((W - s) / 2);
    uint16_t cY = (uint16_t)(top + (bandH - s) / 2);
    uint16_t yt = cY, yb = (uint16_t)(cY + s);

    // Side-card geometry: face width + skew (inner-edge compression at top/bottom).
    uint16_t sw   = (uint16_t)(s * 40 / 100);   // face width of each side card
    uint16_t skew = (uint16_t)(s * 25 / 100);   // top/bottom taper on inner edge

    // ── right side cards (drawn first so centre paints over them) ─────────────
    {
        uint16_t innerX = (uint16_t)(cX + s);
        for (int i = cursor + 1; i < n; i++) {
            if (innerX >= W - 2) break;
            uint16_t outerX = (uint16_t)(innerX + sw);
            if (outerX > W - 1) outerX = (uint16_t)(W - 1);
            if (outerX - innerX < 4) break;
            drawSideCard(c, outerX, innerX, yt, yb, skew, false, m.items[i]);
            innerX = outerX;
        }
    }

    // ── left side cards ───────────────────────────────────────────────────────
    {
        uint16_t innerX = cX;
        for (int i = cursor - 1; i >= 0; i--) {
            if (innerX < sw + 2) break;
            uint16_t outerX = (uint16_t)(innerX - sw);
            if (innerX - outerX < 4) break;
            drawSideCard(c, outerX, innerX, yt, yb, skew, true, m.items[i]);
            innerX = outerX;
        }
    }

    // ── centre card (painted last — covers any side-card overlap) ─────────────
    c.fillRect(cX, cY, s, s, false);           // erase interior to black
    draw::box_rounded(c, cX, cY, s, s, false); // white outline, rounded corners
    {
        const LauncherEntry& e = m.items[cursor];
        if (e.icon && e.iconW && e.iconH && s > 6) {
            uint16_t isz = (uint16_t)(s * 3 / 5);
            uint16_t ix  = (uint16_t)(cX + (s - isz) / 2);
            uint16_t iy  = (uint16_t)(cY + (s - isz) / 2);
            blitScaledMask(c, e.icon, e.iconW, e.iconH, ix, iy, isz, isz, true);
        }
    }

    // ── app name centred below the focused card ───────────────────────────────
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

    // ── position scrollbar ────────────────────────────────────────────────────
    draw::scrollbar(c, 2, (uint16_t)(H - sbH), (uint16_t)(W - 4),
                    (uint16_t)cursor, 1, (uint16_t)n, true);
}

} // namespace nema::shell
