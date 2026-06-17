// Plan 60 Fase 2 — renderer rewrite using tier-1 draw toolkit.
// Pixels are now styled via aether::ui::draw (rounded boxes, dashed scrollbars,
// theme-aware focus). The UiNode tree structure is unchanged (tier-0 kept).
#include "nema/ui/renderer.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/canvas.h"
#include <cstring>

namespace nema::ui {

// Plan 52 — global tick for marquee animation (set by GuiService each frame).
static uint32_t s_renderTick = 0;
void setRenderTick(uint32_t ms) { s_renderTick = ms; }
uint32_t renderTick() { return s_renderTick; }

namespace {

// inFocused: true when this node is a descendant of the focused Pressable.
// Propagated downward so SmartLabel children of a focused row know to marquee.
static void paint(const UiNode* n, Canvas& c, const UiNode* focused, bool inFocused = false) {
    const Style& s = n->style;

    // Background: rounded fill instead of plain rect
    if (s.background)
        aether::ui::draw::box_rounded(c, n->x, n->y, n->w, n->h, true);

    // Border: rounded outline
    if (s.border)
        aether::ui::draw::box_rounded(c, n->x, n->y, n->w, n->h, false);

    if (n->type == NodeType::Text && n->text) {
        FontSpec fs = fontForRole(n->role);
        c.setFont(*fs.font);
        uint16_t tx = (uint16_t)(n->x + s.padding);
        uint16_t ty = (uint16_t)(n->y + s.padding);
        bool on = !s.background;

        if (n->role == TextRole::Smart) {
            // Smart text: marquee when parent Pressable is focused, ellipsis otherwise.
            uint16_t availW = (n->w > 2u * s.padding)
                ? (uint16_t)(n->w - 2u * s.padding) : n->w;
            if (inFocused)
                aether::ui::draw::marquee(c, tx, ty, availW, n->text, s_renderTick);
            else
                aether::ui::draw::ellipsis(c, tx, ty, availW, n->text);
        } else {
            // Text on a filled background renders inverted (white) so banner/title
            // bars are legible. Otherwise normal (black-on-white).
            if (fs.scale <= 1) c.drawText(tx, ty, n->text, on);
            else               c.drawTextScaled(tx, ty, n->text, fs.scale, on);
        }
    }

    if (n->type == NodeType::Icon && n->iconBitmap) {
        uint16_t ix = (uint16_t)(n->x + s.padding);
        uint16_t iy = (uint16_t)(n->y + s.padding);
        aether::ui::draw::icon(c, ix, iy, n->iconBitmap, n->iconW, n->iconH);
        return;
    }

    if (n->type == NodeType::Slider) {
        int range = n->sliderMax - n->sliderMin;
        int val   = n->sliderValue ? *n->sliderValue : n->sliderMin;
        if (val < n->sliderMin) val = n->sliderMin;
        if (val > n->sliderMax) val = n->sliderMax;
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
        if (focused && n == focused) c.invertRect(n->x, n->y, n->w, n->h);
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
        // Dashed scrollbar (Plan 60: aether::ui::draw)
        if (n->scroll && n->scroll->contentMain > n->scroll->viewportMain &&
            n->style.dir == FlexDir::Col && n->scroll->viewportMain > 0) {
            const ScrollState& sc = *n->scroll;
            uint16_t bx = (uint16_t)(n->x + n->w - 3);
            uint16_t total = (uint16_t)(sc.contentMain > 0
                ? (n->h * sc.viewportMain / sc.contentMain) : 1);
            if (total < 1) total = 1;
            // Compute pos from scroll offset
            int32_t maxScroll = sc.maxScroll();
            uint16_t pos = (maxScroll > 0)
                ? (uint16_t)((uint32_t)sc.scrollMain * (n->h - 4) / maxScroll)
                : 0;
            aether::ui::draw::scrollbar(c, bx, n->y, n->h,
                                        sc.scrollMain,
                                        (uint16_t)(maxScroll > 0 ? maxScroll : 1),
                                        false);
            (void)total; (void)pos;
        }
        if (focused && n == focused) c.invertRect(n->x, n->y, n->w, n->h);
        return;
    }

    bool childIn = inFocused || (focused && n == focused);
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        paint(k, c, focused, childIn);

    // Focus highlight: invert the focused Pressable's bounding box
    if (focused && n == focused)
        c.invertRect(n->x, n->y, n->w, n->h);
}

} // anon

void render(const UiNode& root, Canvas& c, const UiNode* focused) {
    c.clearClip();
    paint(&root, c, focused);
    c.clearClip();
}

} // namespace nema::ui
