#include "nema/ui/renderer.h"
#include "nema/ui/text_style.h"
#include "nema/ui/canvas.h"

namespace nema::ui {

static void paint(const UiNode* n, Canvas& c, const UiNode* focused) {
    const Style& s = n->style;

    if (s.background) c.fillRect(n->x, n->y, n->w, n->h, true);
    if (s.border)     c.drawRect(n->x, n->y, n->w, n->h, true);

    if (n->type == NodeType::Text && n->text) {
        FontSpec fs = fontForRole(n->role);
        c.setFont(*fs.font);
        uint16_t tx = (uint16_t)(n->x + s.padding);
        uint16_t ty = (uint16_t)(n->y + s.padding);
        if (fs.scale <= 1) c.drawText(tx, ty, n->text);
        else               c.drawTextScaled(tx, ty, n->text, fs.scale);
    }

    // Slider: track + filled portion + knob, value mapped over the node width.
    if (n->type == NodeType::Slider) {
        int range = n->sliderMax - n->sliderMin;
        int val   = n->sliderValue ? *n->sliderValue : n->sliderMin;
        if (val < n->sliderMin) val = n->sliderMin;
        if (val > n->sliderMax) val = n->sliderMax;
        uint16_t midY = (uint16_t)(n->y + n->h / 2);
        c.fillRect(n->x, midY, n->w, 1, true);                 // track
        uint16_t knobX = (uint16_t)(n->x + (range > 0
                            ? (uint32_t)(val - n->sliderMin) * (n->w - 4) / range : 0));
        c.fillRect(n->x, midY, (uint16_t)(knobX - n->x), 1, true);   // fill
        c.fillRect(knobX, (uint16_t)(n->y + 1), 4, (uint16_t)(n->h - 2), true); // knob
        if (focused && n == focused) c.invertRect(n->x, n->y, n->w, n->h);
        return;
    }

    // Scroll container: clip children to the viewport so overflow is hidden,
    // then draw a proportional scrollbar on the trailing edge.
    if (n->type == NodeType::Scroll) {
        uint16_t ox, oy, ow, oh;
        c.getClip(ox, oy, ow, oh);               // save outer clip
        // Intersect the node box with the outer clip (don't draw past parents).
        uint16_t cx = n->x > ox ? n->x : ox;
        uint16_t cy = n->y > oy ? n->y : oy;
        uint16_t cr = (uint16_t)(n->x + n->w); uint16_t or_ = (uint16_t)(ox + ow);
        uint16_t cb = (uint16_t)(n->y + n->h); uint16_t ob = (uint16_t)(oy + oh);
        if (cr > or_) cr = or_;
        if (cb > ob)  cb = ob;
        if (cr > cx && cb > cy) {
            c.setClip(cx, cy, (uint16_t)(cr - cx), (uint16_t)(cb - cy));
            for (UiNode* k = n->firstChild; k; k = k->nextSibling)
                paint(k, c, focused);
            c.setClip(ox, oy, ow, oh);           // restore before the scrollbar
        }
        // Scrollbar (vertical scroll only): track + proportional thumb.
        if (n->scroll && n->scroll->contentMain > n->scroll->viewportMain &&
            n->style.dir == FlexDir::Col && n->scroll->viewportMain > 0) {
            const ScrollState& sc = *n->scroll;
            uint16_t bx = (uint16_t)(n->x + n->w - 2);   // 2px bar on right edge
            uint16_t vy = n->y;
            uint16_t vh = sc.viewportMain;
            uint16_t thumbH = (uint16_t)((uint32_t)vh * vh / sc.contentMain);
            if (thumbH < 4) thumbH = 4;
            uint16_t maxOff = (uint16_t)(vh - thumbH);
            int16_t maxS = sc.maxScroll();
            uint16_t thumbY = maxS > 0
                ? (uint16_t)(vy + (uint32_t)maxOff * sc.scrollMain / maxS)
                : vy;
            c.fillRect(bx, thumbY, 2, thumbH, true);
        }
        // Focus highlight still applies to the scroll node box itself if focused.
        if (focused && n == focused) c.invertRect(n->x, n->y, n->w, n->h);
        return;
    }

    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        paint(k, c, focused);

    // Focus highlight drawn last (XOR over the node's box).
    if (focused && n == focused)
        c.invertRect(n->x, n->y, n->w, n->h);
}

void render(const UiNode& root, Canvas& c, const UiNode* focused) {
    c.clearClip();
    paint(&root, c, focused);
    c.clearClip();
}

} // namespace nema::ui
