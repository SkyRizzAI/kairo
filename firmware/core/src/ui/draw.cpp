// Plan 60 Fase 1 — tier-1 draw toolkit implementation.
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/font_registry.h"
#include <cstring>
#include <cstdlib>

namespace aether::ui::draw {

using nema::Canvas;
using aether::ui::TextRole;
using aether::ui::FontSpec;
using nema::display::FontHandle;
using nema::display::FontRegistry;
using aether::ui::fontForRole;
using aether::ui::measureTextW;
using aether::ui::measureTextH;

// ── Internal helpers ──────────────────────────────────────────────────────────

static const nema::display::BitmapFont& resolve(nema::display::FontHandle h) {
    return *FontRegistry::instance().get(h);
}

static void drawText(Canvas& c, uint16_t x, uint16_t y, const char* s,
                     const FontSpec& fs, bool on = true) {
    c.setFont(fs.handle);
    if (fs.scale <= 1) c.drawText(x, y, s, on);
    else               c.drawTextScaled(x, y, s, fs.scale, on);
}

// Draw `len` chars from `text` at (x,y). Uses a stack buffer; truncates at 63 chars.
static void drawSpan(Canvas& c, uint16_t x, uint16_t y,
                     const char* text, size_t len, const FontSpec& fs, bool on = true) {
    if (!text || len == 0) return;
    char buf[64];
    if (len > 63) len = 63;
    memcpy(buf, text, len);
    buf[len] = '\0';
    drawText(c, x, y, buf, fs, on);
}

// ── Primitives ────────────────────────────────────────────────────────────────

void frame(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    c.drawRect(x, y, w, h);
}

void box(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool inverted) {
    if (inverted) c.invertRect(x, y, w, h);
    else          c.fillRect(x, y, w, h);
}

void box_rounded(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool filled) {
    if (w < 2 || h < 2) return;
    if (filled) {
        c.fillRect(x, y, w, h);
        // Cut 4 corners
        c.drawPixel(x,           y,           false);
        c.drawPixel(x + w - 1,   y,           false);
        c.drawPixel(x,           y + h - 1,   false);
        c.drawPixel(x + w - 1,   y + h - 1,   false);
    } else {
        c.drawRect(x, y, w, h);
        // Cut 4 corners
        c.drawPixel(x,           y,           false);
        c.drawPixel(x + w - 1,   y,           false);
        c.drawPixel(x,           y + h - 1,   false);
        c.drawPixel(x + w - 1,   y + h - 1,   false);
    }
}

void separator(Canvas& c, uint16_t x, uint16_t y, uint16_t len, bool horizontal) {
    if (horizontal) c.fillRect(x, y, len, 1);
    else            c.fillRect(x, y, 1, len);
}

// ── Scrollbar ─────────────────────────────────────────────────────────────────

void scrollbar(Canvas& c, uint16_t x, uint16_t y, uint16_t size,
               uint16_t scrollOffset, uint16_t viewport, uint16_t content,
               bool horizontal) {
    if (size == 0) return;
    const uint16_t BAR = 3;        // bar thickness in px
    const uint16_t MIN_THUMB = 4;  // smallest the thumb is allowed to get

    // --- Dashed track (1px dots every 2px along the axis) ---
    if (horizontal) {
        for (uint16_t i = 0; i < size; i += 2)
            c.fillRect((uint16_t)(x + i), (uint16_t)(y + 1), 1, 1);
    } else {
        for (uint16_t i = 0; i < size; i += 2)
            c.fillRect((uint16_t)(x + 1), (uint16_t)(y + i), 1, 1);
    }

    // --- Solid thumb ---
    // Nothing to scroll → the thumb fills the whole track.
    if (content <= viewport || viewport == 0) {
        if (horizontal) c.fillRect(x, y, size, BAR);
        else            c.fillRect(x, y, BAR, size);
        return;
    }

    // Thumb length is proportional to the visible fraction, clamped to a floor
    // so it stays grabbable on very long content.
    uint16_t thumb = (uint16_t)((uint32_t)size * viewport / content);
    if (thumb < MIN_THUMB) thumb = MIN_THUMB;
    if (thumb > size)      thumb = size;

    // Position maps scrollOffset∈[0,maxScroll] onto travel∈[0,size-thumb].
    // Using maxScroll (not maxScroll-1) as the denominator guarantees the thumb's
    // far edge lands exactly on the track end at full scroll — never past it.
    uint16_t maxScroll = (uint16_t)(content - viewport);
    uint16_t off       = scrollOffset > maxScroll ? maxScroll : scrollOffset;
    uint16_t travel    = (uint16_t)(size - thumb);
    uint16_t thumbOff  = maxScroll ? (uint16_t)((uint32_t)off * travel / maxScroll) : 0;

    if (horizontal) c.fillRect((uint16_t)(x + thumbOff), y, thumb, BAR);
    else            c.fillRect(x, (uint16_t)(y + thumbOff), BAR, thumb);
}

// ── Text helpers ──────────────────────────────────────────────────────────────

void multiline(Canvas& c, uint16_t x, uint16_t y, uint16_t w,
               const char* text, TextRole role) {
    if (!text || !*text || w == 0) return;
    FontSpec fs = fontForRole(role);
    uint16_t charW_px = (uint16_t)((resolve(fs.handle).charW + resolve(fs.handle).spacing) * fs.scale);
    uint16_t lineH    = (uint16_t)(resolve(fs.handle).charH * fs.scale + 1);
    uint16_t cy = y;

    size_t len = strlen(text);
    size_t lineStart = 0;
    size_t lastBreak = (size_t)-1;  // index of last safe line-break (space)
    uint16_t lineW   = 0;

    for (size_t i = 0; i <= len; i++) {
        char ch = text[i];
        bool flush = (ch == '\n' || ch == '\0');

        if (!flush) {
            if (ch == ' ') lastBreak = i;
            lineW += charW_px;
        }

        if (flush || lineW > w) {
            size_t breakAt = flush ? i
                           : (lastBreak != (size_t)-1 ? lastBreak : i);
            size_t lineLen = breakAt - lineStart;
            if (lineLen > 0)
                drawSpan(c, x, cy, text + lineStart, lineLen, fs);
            cy += lineH;

            if (!flush) {
                size_t newStart = (lastBreak != (size_t)-1) ? lastBreak + 1 : i;
                lineStart = newStart;
                lastBreak = (size_t)-1;
                // Recount lineW from new lineStart up to and including i
                lineW = (i >= lineStart) ? (uint16_t)((i - lineStart + 1) * charW_px) : 0;
            } else {
                lineStart = i + 1;
                lastBreak = (size_t)-1;
                lineW = 0;
            }
        }
    }
}

uint16_t measureMultilineH(const char* text, uint16_t w, TextRole role) {
    if (!text || !*text || w == 0) return measureTextH(role);
    FontSpec fs = fontForRole(role);
    uint16_t charW_px = (uint16_t)((resolve(fs.handle).charW + resolve(fs.handle).spacing) * fs.scale);
    uint16_t lineH    = (uint16_t)(resolve(fs.handle).charH * fs.scale + 1);

    size_t len = strlen(text);
    size_t lineStart = 0;
    size_t lastBreak = (size_t)-1;
    uint16_t lineW  = 0;
    uint16_t lines  = 0;

    for (size_t i = 0; i <= len; i++) {
        char ch = text[i];
        bool flush = (ch == '\n' || ch == '\0');
        if (!flush) {
            if (ch == ' ') lastBreak = i;
            lineW += charW_px;
        }
        if (flush || lineW > w) {
            size_t breakAt = flush ? i
                           : (lastBreak != (size_t)-1 ? lastBreak : i);
            size_t lineLen = breakAt - lineStart;
            if (lineLen > 0 || flush) lines++;

            if (!flush) {
                size_t newStart = (lastBreak != (size_t)-1) ? lastBreak + 1 : i;
                lineStart = newStart;
                lastBreak = (size_t)-1;
                lineW = (i >= lineStart) ? (uint16_t)((i - lineStart + 1) * charW_px) : 0;
            } else {
                lineStart = i + 1;
                lineW = 0;
                lastBreak = (size_t)-1;
            }
        }
    }
    if (lines == 0) lines = 1;
    return (uint16_t)(lines * lineH);
}

void marquee(Canvas& c, uint16_t x, uint16_t y, uint16_t w,
             const char* text, uint32_t tick, TextRole role) {
    if (!text || !*text) return;
    FontSpec fs = fontForRole(role);
    const nema::display::BitmapFont& f = resolve(fs.handle);
    uint16_t textW = measureTextW(text, role);
    uint16_t lineH = measureTextH(role);

    if (textW <= w) {
        drawText(c, x, y, text, fs);
        return;
    }

    const uint16_t GAP = 20;
    uint32_t cycle    = (uint32_t)textW + GAP;
    // ~40px/sec: divide ms tick by 25.  scrollPx always increases → no stutter.
    uint32_t scrollPx = (tick / 25) % cycle;

    // Walk actual per-character widths to find the first visible character.
    uint32_t cum = 0;
    size_t   slen = strlen(text);
    size_t   firstChar = 0;
    for (; firstChar < slen; ++firstChar) {
        uint8_t  ci = (text[firstChar] >= ' ') ? (uint8_t)(text[firstChar] - ' ') : 0;
        uint16_t cw = (uint16_t)(((f.widths ? f.widths[ci] : f.charW) + f.spacing) * fs.scale);
        if (cum + cw > scrollPx) break;
        cum += cw;
    }
    uint32_t intoChar = scrollPx - cum;
    uint16_t drawX    = (intoChar <= (uint32_t)x) ? (uint16_t)(x - intoChar) : 0;

    // Draw two copies of the text separated by `cycle` pixels so the loop is
    // seamless — like an LED ticker/train: as copy 1 exits the left edge, copy 2
    // enters from the right with no gap or jump.
    // textLeft: virtual screen x of the full text's left edge (may be off-screen left).
    // copy 2 begins one `cycle` to the right — always positive since scrollPx < cycle.
    int textLeft = (int)x - (int)scrollPx;
    c.setClip(x, y, w, lineH);
    if (firstChar < slen)
        drawText(c, drawX, y, text + firstChar, fs);             // copy 1: tail still on screen
    drawText(c, (uint16_t)(textLeft + (int)cycle), y, text, fs); // copy 2: full text, one cycle ahead
    c.clearClip();
}

void ellipsis(Canvas& c, uint16_t x, uint16_t y, uint16_t w,
              const char* text, TextRole role) {
    if (!text || !*text) return;
    FontSpec fs = fontForRole(role);
    const nema::display::BitmapFont& f = resolve(fs.handle);
    uint16_t textW = measureTextW(text, role);

    if (textW <= w) {
        drawText(c, x, y, text, fs);
        return;
    }

    uint16_t dotsW = measureTextW("...", role);
    if (dotsW >= w) {
        drawText(c, x, y, "...", fs);
        return;
    }

    uint16_t available = w - dotsW;
    // Walk actual per-character widths so "..." is placed right after the last
    // visible glyph — fixes the large gap on proportional fonts (same as marquee).
    size_t   slen = strlen(text);
    size_t   n    = 0;
    uint32_t usedW = 0;
    for (; n < slen; ++n) {
        uint8_t  ci = (text[n] >= ' ') ? (uint8_t)(text[n] - ' ') : 0;
        uint16_t cw = (uint16_t)(((f.widths ? f.widths[ci] : f.charW) + f.spacing) * fs.scale);
        if (usedW + cw > available) break;
        usedW += cw;
    }
    if (n == 0) { drawText(c, x, y, "...", fs); return; }

    drawSpan(c, x, y, text, n, fs);
    drawText(c, (uint16_t)(x + usedW), y, "...", fs);
}

// ── Icon ──────────────────────────────────────────────────────────────────────

void icon(Canvas& c, uint16_t x, uint16_t y,
          const uint8_t* bitmap, uint8_t w_px, uint8_t h_px) {
    if (!bitmap || w_px == 0 || h_px == 0) return;
    c.drawBitmap(x, y, w_px, h_px, bitmap);
}

// ── Chrome / DSi-style ────────────────────────────────────────────────────────

void banner(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
            const char* title, bool notch) {
    // Filled dark bar
    c.fillRect(x, y, w, h);

    // Centered title — white text (on=false on filled background)
    if (title && *title) {
        FontSpec fs = fontForRole(aether::ui::TextRole::Title);
        uint16_t tw = measureTextW(title, aether::ui::TextRole::Title);
        uint16_t th = measureTextH(aether::ui::TextRole::Title);
        uint16_t tx = (tw < w) ? (uint16_t)(x + (w - tw) / 2) : x;
        uint16_t ty = (th < h) ? (uint16_t)(y + (h - th) / 2) : y;
        drawText(c, tx, ty, title, fs, false);
    }

    // Notch indicator: 3-pixel wide chevron below banner center
    if (notch && h >= 2) {
        uint16_t mx = (uint16_t)(x + w / 2);
        uint16_t ny = (uint16_t)(y + h);  // just below banner
        // Draw a small ▼ shape: 3 pixels wide, 2 pixels tall
        c.drawPixel(mx,       ny,     true);
        c.drawPixel((uint16_t)(mx - 1), (uint16_t)(ny - 1), true);
        c.drawPixel((uint16_t)(mx + 1), (uint16_t)(ny - 1), true);
    }
}

void posbar(Canvas& c, uint16_t x, uint16_t y, uint16_t w,
            uint16_t pos, uint16_t total) {
    if (total == 0) return;
    const uint16_t DOT = 3;   // dot size in px
    const uint16_t GAP = 2;   // gap between dots

    uint16_t totalW = (uint16_t)(total * DOT + (total > 1 ? (total - 1) * GAP : 0));
    uint16_t startX = (totalW < w) ? (uint16_t)(x + (w - totalW) / 2) : x;

    for (uint16_t i = 0; i < total; i++) {
        uint16_t dx = (uint16_t)(startX + i * (DOT + GAP));
        if (i == pos) {
            // Filled dot
            c.fillRect(dx, y, DOT, DOT);
        } else {
            // Outline dot
            c.drawRect(dx, y, DOT, DOT);
        }
    }
}

} // namespace aether::ui::draw
