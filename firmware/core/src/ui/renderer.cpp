// Plan 60 Fase 2 — renderer rewrite using tier-1 draw toolkit.
// Pixels are now styled via aether::ui::draw (rounded boxes, dashed scrollbars,
// theme-aware focus). The UiNode tree structure is unchanged (tier-0 kept).
#include "nema/ui/renderer.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/animation_player.h"
#include "nema/ui/canvas.h"
#include "nema/ui/node.h"
#include "nema/ui/ui_constants.h"
#include <cstring>

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

// Plan 52 — global tick for marquee animation (set by GuiService each frame).
static uint32_t s_renderTick = 0;
void setRenderTick(uint32_t ms) { s_renderTick = ms; }
uint32_t renderTick() { return s_renderTick; }

namespace {

// Focus highlight. Plain square invert by default; rounded (r=1) inverted box
// when the node opts into Style::selectBox (ListView rows, Flipper style).
static void highlightBox(Canvas& c, const UiNode* n) {
    uint8_t r = n->style.cornerRadius;
    if (n->style.selectBox && n->w >= 10 && n->h >= 3) {
        // The focus fill spans the full row. No per-row scrollbar gutter is needed: the
        // scroll container reserves the scrollbar strip in layout (arrangeScroll), so the
        // row is already narrowed to clear the bar when it's shown, and full-width when not.
        uint16_t w = n->w;
        c.invertRect(n->x, (uint16_t)(n->y + 1), w, (uint16_t)(n->h - 2)); // middle band
        c.invertRect((uint16_t)(n->x + 1), n->y, (uint16_t)(w - 2), 1);    // top cap
        c.invertRect((uint16_t)(n->x + 1), (uint16_t)(n->y + n->h - 1), (uint16_t)(w - 2), 1); // bottom cap
    } else if (r > 1 && n->w > 2 * r && n->h > 2 * r) {
        // Rounded invert matching the border radius (Mission Control tiles/bars):
        // same shape, just filled + the icon flips to the opposite colour.
        c.invertRect(n->x, (uint16_t)(n->y + r), n->w, (uint16_t)(n->h - 2 * r));
        for (uint8_t i = 0; i < r; i++) {
            uint16_t inset = (uint16_t)(r - i);
            uint16_t bw    = (uint16_t)(n->w - 2 * inset);
            c.invertRect((uint16_t)(n->x + inset), (uint16_t)(n->y + i),         bw, 1);
            c.invertRect((uint16_t)(n->x + inset), (uint16_t)(n->y + n->h - 1 - i), bw, 1);
        }
    } else if (n->style.border && n->w > 2 && n->h > 2) {
        // Bordered control (e.g. a Dialog button): invert only the INTERIOR so the outline
        // stays drawn in both states — focused and unfocused keep the same border/size,
        // just the inside fills (and child text flips). Without this the full invert eats
        // the border on focus, so the button looks like it changes size.
        c.invertRect((uint16_t)(n->x + 1), (uint16_t)(n->y + 1),
                     (uint16_t)(n->w - 2), (uint16_t)(n->h - 2));
    } else {
        c.invertRect(n->x, n->y, n->w, n->h);
    }
}

// inFocused: true when this node is a descendant of the focused Pressable.
// Propagated downward so SmartLabel children of a focused row know to marquee.
static void paint(const UiNode* n, Canvas& c, const UiNode* focused,
                  bool inFocused = false) {
    const Style& s = n->style;

    // Background / border: rounded-rect with the node's corner radius (default 1).
    if (s.background)
        c.fillRoundRect(n->x, n->y, n->w, n->h, s.cornerRadius, true);
    if (s.border)
        c.drawRoundRect(n->x, n->y, n->w, n->h, s.cornerRadius, true);

    if (n->type == NodeType::Text && n->text) {
        FontSpec fs = fontForRole(n->role);
        c.setFont(fs.handle);
        uint16_t tx = (uint16_t)(n->x + s.padding);
        uint16_t ty = (uint16_t)(n->y + s.padding);
        bool on = !s.background;

        // Overflow:hidden — clip drawing to the node's bbox (intersected with the
        // current clip). Used by the collapsing footer-legend label.
        uint16_t svx = 0, svy = 0, svw = 0, svh = 0; bool clipped = false;
        if (s.clip) {
            c.getClip(svx, svy, svw, svh);
            uint16_t x0 = n->x, y0 = n->y;
            uint16_t x1 = (uint16_t)(n->x + n->w), y1 = (uint16_t)(n->y + n->h);
            uint16_t cr = (uint16_t)(svx + svw), cb = (uint16_t)(svy + svh);
            if (x0 < svx) x0 = svx;
            if (y0 < svy) y0 = svy;
            if (x1 > cr)  x1 = cr;
            if (y1 > cb)  y1 = cb;
            if (x1 > x0 && y1 > y0) c.setClip(x0, y0, (uint16_t)(x1 - x0), (uint16_t)(y1 - y0));
            else                    c.setClip(0, 0, 0, 0);   // fully collapsed → draw nothing
            clipped = true;
        }

        if (n->role == TextRole::Smart) {
            // Smart text: static when fits, marquee when focused+overflow, ellipsis otherwise.
            uint16_t availW = (n->w > 2u * s.padding)
                ? (uint16_t)(n->w - 2u * s.padding) : n->w;
            uint16_t textW  = aether::ui::measureTextW(n->text, TextRole::Body);
            if (textW <= availW) {
                // Text fits — draw once, statically. Never marquee a fitting label
                // (copy 2 would land inside the clip and show a double image).
                if (s.justify == Justify::Center) {
                    uint16_t cx = (uint16_t)(tx + (availW - textW) / 2);
                    c.drawText(cx, ty, n->text, on);
                } else {
                    c.drawText(tx, ty, n->text, on);
                }
            } else if (inFocused) {
                aether::ui::draw::marquee(c, tx, ty, availW, n->text, s_renderTick);
            } else {
                aether::ui::draw::ellipsis(c, tx, ty, availW, n->text);
            }
        } else {
            // Text on a filled background renders inverted (white) so banner/title
            // bars are legible. Otherwise normal (black-on-white).
            if (n->wrap && n->w > 0) {
                // F2.3 Multiline: word-wrap within node width
                uint16_t availW = (n->w > 2u * s.padding) ? (uint16_t)(n->w - 2u * s.padding) : n->w;
                aether::ui::draw::multiline(c, tx, ty, availW, n->text, n->role);
            } else {
                // Single-line (existing behavior)
                if (fs.scale <= 1) c.drawText(tx, ty, n->text, on);
                else               c.drawTextScaled(tx, ty, n->text, fs.scale, on);
            }
        }
        if (clipped) c.setClip(svx, svy, svw, svh);   // restore prior clip
    }

    if (n->type == NodeType::Icon && n->iconBitmap) {
        uint16_t ix = (uint16_t)(n->x + s.padding);
        uint16_t iy = (uint16_t)(n->y + s.padding);
        uint8_t sc = n->iconScale ? n->iconScale : 1;
        if (s.background) {
            // Icon on a filled box (e.g. a footer-legend pill): XOR the set bits
            // so the glyph shows in paper colour. Mirrors inverted Text drawing
            // (on = !s.background) so icon + label match on a dark capsule.
            for (uint8_t r2 = 0; r2 < n->iconH; r2++)
                for (uint8_t cc = 0; cc < n->iconW; cc++) {
                    uint32_t bit = (uint32_t)r2 * n->iconW + cc;
                    if ((n->iconBitmap[bit / 8] >> (7 - (bit % 8))) & 1)
                        c.invertRect((uint16_t)(ix + cc * sc), (uint16_t)(iy + r2 * sc), sc, sc);
                }
        } else if (sc <= 1) {
            aether::ui::draw::icon(c, ix, iy, n->iconBitmap, n->iconW, n->iconH);
        } else {
            for (uint8_t r2 = 0; r2 < n->iconH; r2++)
                for (uint8_t cc = 0; cc < n->iconW; cc++) {
                    uint32_t bit = (uint32_t)r2 * n->iconW + cc;
                    if ((n->iconBitmap[bit / 8] >> (7 - (bit % 8))) & 1)
                        c.fillRect((uint16_t)(ix + cc * sc), (uint16_t)(iy + r2 * sc), sc, sc, true);
                }
        }
        return;
    }

    // Plan 70: AnimatedIcon — draw the current frame from the player.
    if (n->type == NodeType::AnimatedIcon && n->animPlayer) {
        uint16_t ix = (uint16_t)(n->x + s.padding);
        uint16_t iy = (uint16_t)(n->y + s.padding);
        aether::ui::draw::icon(c, ix, iy,
            n->animPlayer->currentFrameData(),
            (uint8_t)n->animPlayer->width(),
            (uint8_t)n->animPlayer->height());
        return;
    }

    if (n->type == NodeType::Switch) {
        uint16_t x = n->x, y = n->y, w = n->w, h = n->h;
        if (w >= 8 && h >= 5) {
            uint8_t  r  = (h >= 8) ? 3 : 2;          // rounded ends, not pointy
            uint16_t kd = (uint16_t)(h - 4);          // knob diameter (2px inset top/bottom)
            uint8_t  kr = (uint8_t)(kd / 2);
            uint16_t ky = (uint16_t)(y + 2);
            if (n->switchOn) {
                c.fillRoundRect(x, y, w, h, r, true);                    // filled track
                uint16_t kx = (uint16_t)(x + w - 2 - kd);               // knob HOLE, right
                c.fillRoundRect(kx, ky, kd, kd, kr, false);
            } else {
                c.drawRoundRect(x, y, w, h, r, true);                    // outline track
                uint16_t kx = (uint16_t)(x + 2);                        // filled knob, left
                c.fillRoundRect(kx, ky, kd, kd, kr, true);
            }
        }
        return;
    }

    if (n->type == NodeType::Spinner) {
        uint16_t cx = (uint16_t)(n->x + n->w / 2);
        uint16_t cy = (uint16_t)(n->y + n->h / 2);
        uint16_t r  = (uint16_t)((n->w < n->h ? n->w : n->h) / 2);
        if (r > 1) aether::ui::draw::spinner(c, cx, cy, (uint16_t)(r - 1), s_renderTick);
        return;
    }

    if (n->type == NodeType::Progress) {
        uint16_t x = n->x, y = n->y, w = n->w, h = n->h;
        if (w >= 4 && h >= 3) {
            c.drawRoundRect(x, y, w, h, 2, true);                        // outline track
            int pct = n->progressPct > 100 ? 100 : n->progressPct;
            int fillW = (int)(w - 4) * pct / 100;                        // 2px inset each side
            if (fillW > 0)
                c.fillRoundRect((uint16_t)(x + 2), (uint16_t)(y + 2), (uint16_t)fillW, (uint16_t)(h - 4), 1, true);
        }
        return;
    }

    if (n->type == NodeType::Slider) {
        int range = n->sliderMax - n->sliderMin;
        int val   = n->sliderValue ? *n->sliderValue : n->sliderMin;
        if (val < n->sliderMin) val = n->sliderMin;
        if (val > n->sliderMax) val = n->sliderMax;
        if (n->sliderVertical) {
            // iOS-style pill bar: rounded outline, solid rounded fill from the bottom,
            // with a centred glyph (XOR so it shows on both the fill and the track).
            uint8_t rr = n->style.cornerRadius > 1 ? n->style.cornerRadius : (uint8_t)(n->w / 3);
            if (rr < 2) rr = 2;
            c.drawRoundRect(n->x, n->y, n->w, n->h, rr, true);
            uint16_t fillH = (uint16_t)(range > 0
                ? (uint32_t)(val - n->sliderMin) * (n->h - 2) / range : 0);
            if (fillH > 0) {
                uint16_t fy = (uint16_t)(n->y + n->h - 1 - fillH);
                c.fillRoundRect((uint16_t)(n->x + 1), fy, (uint16_t)(n->w - 2), fillH, rr, true);
            }
            if (n->iconBitmap) {
                uint16_t gx = (uint16_t)(n->x + (n->w - n->iconW) / 2);
                uint16_t gy = (uint16_t)(n->y + (n->h - n->iconH) / 2);
                for (uint8_t r2 = 0; r2 < n->iconH; r2++)
                    for (uint8_t cc = 0; cc < n->iconW; cc++) {
                        uint32_t bit = (uint32_t)r2 * n->iconW + cc;
                        if ((n->iconBitmap[bit / 8] >> (7 - (bit % 8))) & 1)
                            c.invertRect((uint16_t)(gx + cc), (uint16_t)(gy + r2), 1, 1);
                    }
            }
            if (focused && n == focused) highlightBox(c, n);
            return;
        }
        uint16_t midY = (uint16_t)(n->y + n->h / 2);
        // Track as separator
        aether::ui::draw::separator(c, n->x, midY, n->w, true);
        // Filled portion
        uint16_t fillW = (uint16_t)(range > 0
            ? (uint32_t)(val - n->sliderMin) * (n->w - 4) / range : 0);
        c.fillRect(n->x, midY, fillW, 1);
        // Knob: rounded 4px block
        uint16_t knobX = (uint16_t)(n->x + fillW);
        aether::ui::draw::box_rounded(c, knobX, (uint16_t)(n->y + 1), 4,
                                      (uint16_t)(n->h - 2), true);
        if (focused && n == focused) highlightBox(c, n);
        return;
    }

    if (n->type == NodeType::Scroll) {
        uint16_t ox, oy, ow, oh;
        c.getClip(ox, oy, ow, oh);
        uint16_t cx = n->x > ox ? n->x : ox;
        uint16_t cy = n->y > oy ? n->y : oy;
        uint16_t cr = (uint16_t)(n->x + n->w); uint16_t or_ = (uint16_t)(ox + ow);
        uint16_t cb = (uint16_t)(n->y + n->h); uint16_t ob  = (uint16_t)(oy + oh);
        if (cr > or_) cr = or_;
        if (cb > ob)  cb = ob;
        if (cr > cx && cb > cy) {
            c.setClip(cx, cy, (uint16_t)(cr - cx), (uint16_t)(cb - cy));
            bool childIn = inFocused || (focused && n == focused);
            for (UiNode* k = n->firstChild; k; k = k->nextSibling)
                paint(k, c, focused, childIn);
            c.setClip(ox, oy, ow, oh);
        }
        // Dashed scrollbar (Plan 60: aether::ui::draw). Thumb size/position derive
        // from the real pixel lengths: track = viewport, total content, scroll offset.
        if (n->scroll && n->scroll->contentMain > n->scroll->viewportMain &&
            n->style.dir == FlexDir::Col && n->scroll->viewportMain > 0) {
            const ScrollState& sc = *n->scroll;
            uint16_t bx = (uint16_t)(n->x + n->w - nema::display::SCROLLBAR_BAR_INSET);
            aether::ui::draw::scrollbar(c, bx, n->y, n->h,
                                        (uint16_t)(sc.scrollMain < 0 ? 0 : sc.scrollMain),
                                        sc.viewportMain,
                                        sc.contentMain,
                                        false);
        }
        if (focused && n == focused) highlightBox(c, n);
        return;
    }

    bool childIn = inFocused || (focused && n == focused) || n->selfHighlight;
    // F2.4 Absolute positioning — painter's algorithm: relative children first,
    // then absolute children on top.
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        if (k->style.position == Position::Relative)
            paint(k, c, focused, childIn);
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        if (k->style.position == Position::Absolute)
            paint(k, c, focused, childIn);

    // Focus highlight: invert the focused Pressable's bounding box.
    // selfHighlight covers VirtualList items (focusable=false, pointer match impossible).
    if ((focused && n == focused) || n->selfHighlight)
        highlightBox(c, n);
}

} // anon

void render(const UiNode& root, Canvas& c, const UiNode* focused) {
    c.clearClip();
    paint(&root, c, focused);
    c.clearClip();
}

} // namespace aether::ui
