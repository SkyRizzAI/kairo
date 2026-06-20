#include "nema/ui/layout.h"

namespace nema::ui {

static void arrange(UiNode* n);   // fwd: arrangeScroll recurses into it

static int childCount(const UiNode* n) {
    int c = 0;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) c++;
    return c;
}

// ── Pass 1: measure intrinsic content size (bottom-up) ─────────────────────
static void measure(UiNode* n, const TextMetrics& tm) {
    const Style& s = n->style;
    const uint16_t pad2 = (uint16_t)(s.padding * 2);

    if (n->type == NodeType::Text) {
        uint16_t tw = n->text ? tm.width(tm.ctx, n->text, n->role) : 0;
        uint16_t th = tm.height(tm.ctx, n->role);
        n->w = (s.width  == SIZE_AUTO) ? (uint16_t)(tw + pad2) : s.width;
        n->h = (s.height == SIZE_AUTO) ? (uint16_t)(th + pad2) : s.height;
        return;
    }

    if (n->type == NodeType::Slider) {
        constexpr uint16_t SLIDER_H = 11;
        n->w = (s.width  == SIZE_AUTO) ? 0 : s.width;
        n->h = (s.height == SIZE_AUTO) ? SLIDER_H : s.height;
        return;
    }

    if (n->type == NodeType::Icon) {
        // Leaf: fixed size from the bitmap dimensions + padding.
        const uint16_t pad2 = (uint16_t)(s.padding * 2);
        n->w = (s.width  == SIZE_AUTO) ? (uint16_t)(n->iconW + pad2) : s.width;
        n->h = (s.height == SIZE_AUTO) ? (uint16_t)(n->iconH + pad2) : s.height;
        return;
    }

    // View / Pressable / Scroll: measure children first.
    int n_children = 0;
    uint32_t sumMain = 0, maxCross = 0;
    const bool isRow = (s.dir == FlexDir::Row);
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        measure(k, tm);
        uint16_t kMain  = isRow ? k->w : k->h;
        uint16_t kCross = isRow ? k->h : k->w;
        sumMain += kMain;
        if (kCross > maxCross) maxCross = kCross;
        n_children++;
    }
    uint32_t gaps = (n_children > 1) ? (uint32_t)s.gap * (n_children - 1) : 0;

    uint16_t contentCross = (uint16_t)(maxCross + pad2);

    if (n->type == NodeType::Scroll) {
        // The natural inner content length (children only, no node padding) is
        // recorded so arrange() can clamp scroll + the renderer can size the bar.
        if (n->scroll) n->scroll->contentMain = (uint16_t)(sumMain + gaps);
        // Report a BOUNDED main size to the parent flex pass: a fixed size if the
        // style sets one, else 0 so the node claims the viewport via flexGrow
        // instead of forcing the parent to grow to fit all content. This is the
        // flex⇄overflow contract — the parent constrains the viewport, content
        // beyond it scrolls. (RN: <ScrollView style={{flex:1}}/>.)
        uint16_t mainStyle = isRow ? s.width : s.height;
        uint16_t mainSize  = (mainStyle == SIZE_AUTO) ? 0 : mainStyle;
        uint16_t crossSize = (isRow ? s.height : s.width) == SIZE_AUTO
                                 ? contentCross
                                 : (isRow ? s.height : s.width);
        if (isRow) { n->w = mainSize; n->h = crossSize; }
        else       { n->h = mainSize; n->w = crossSize; }
        return;
    }

    uint16_t contentMain  = (uint16_t)(sumMain + gaps + pad2);
    uint16_t cw = isRow ? contentMain : contentCross;
    uint16_t ch = isRow ? contentCross : contentMain;

    n->w = (s.width  == SIZE_AUTO) ? cw : s.width;
    n->h = (s.height == SIZE_AUTO) ? ch : s.height;
}

// ── Scroll arrange: children at natural main size, shifted by -scrollMain ───
static void arrangeScroll(UiNode* n) {
    const Style& s   = n->style;
    const int    pad = s.padding;
    const bool   isRow = (s.dir == FlexDir::Row);

    const int innerX = n->x + pad;
    const int innerY = n->y + pad;
    const int innerW = (int)n->w - 2 * pad;
    const int innerH = (int)n->h - 2 * pad;
    const int viewportMain = isRow ? innerW : innerH;
    const int crossAvail   = isRow ? innerH : innerW;

    const int contentMain = n->scroll ? (int)n->scroll->contentMain : 0;
    int maxS = contentMain > viewportMain ? contentMain - viewportMain : 0;
    int sc = n->scroll ? n->scroll->scrollMain : 0;
    if (sc < 0) sc = 0;
    if (sc > maxS) sc = maxS;
    if (n->scroll) {
        n->scroll->scrollMain   = (int16_t)sc;
        n->scroll->viewportMain = (uint16_t)(viewportMain > 0 ? viewportMain : 0);
    }

    // Lay children stacked at their natural main size, offset by the scroll pos.
    int cursor = (isRow ? innerX : innerY) - sc;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        if (s.align == Align::Stretch) {
            if (isRow) k->h = (uint16_t)crossAvail;
            else       k->w = (uint16_t)crossAvail;
        }
        int crossSize = isRow ? k->h : k->w;
        int crossOff = 0;
        switch (s.align) {
            case Align::Start:   crossOff = 0; break;
            case Align::Center:  crossOff = (crossAvail - crossSize) / 2; break;
            case Align::End:     crossOff = crossAvail - crossSize; break;
            case Align::Stretch: crossOff = 0; break;
        }
        if (crossOff < 0) crossOff = 0;

        if (isRow) { k->x = (int16_t)cursor;             k->y = (int16_t)(innerY + crossOff); }
        else       { k->x = (int16_t)(innerX + crossOff); k->y = (int16_t)cursor; }

        cursor += (isRow ? k->w : k->h) + s.gap;
        arrange(k);
    }
}

// ── Pass 2: arrange (top-down); node x/y/w/h already final ─────────────────
static void arrange(UiNode* n) {
    if (n->type == NodeType::Scroll) { arrangeScroll(n); return; }

    const Style& s = n->style;
    const int pad  = s.padding;
    const bool isRow = (s.dir == FlexDir::Row);

    const int innerX = n->x + pad;
    const int innerY = n->y + pad;
    const int innerW = (int)n->w - 2 * pad;
    const int innerH = (int)n->h - 2 * pad;
    const int mainAvail  = isRow ? innerW : innerH;
    const int crossAvail = isRow ? innerH : innerW;

    int nKids = childCount(n);
    if (nKids == 0) return;

    // flex-basis:0 — grow children that opt in contribute 0 to the base sum, so
    // the leftover (and thus the ratio split) ignores their content width.
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        if (k->style.flexZero && k->style.flexGrow > 0) {
            if (isRow) k->w = 0; else k->h = 0;
        }
    }

    // Sum measured main sizes + collect grow weights.
    int sumMain = 0, growWeight = 0;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        sumMain += isRow ? k->w : k->h;
        growWeight += k->style.flexGrow;
    }
    int totalGap = (nKids > 1) ? s.gap * (nKids - 1) : 0;
    int leftover = mainAvail - sumMain - totalGap;
    if (leftover < 0) leftover = 0;

    // Distribute leftover to grow children (remainder → last grow child).
    if (growWeight > 0 && leftover > 0) {
        int given = 0;
        UiNode* lastGrow = nullptr;
        for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
            if (k->style.flexGrow > 0) {
                int extra = leftover * k->style.flexGrow / growWeight;
                if (isRow) k->w = (uint16_t)(k->w + extra);
                else       k->h = (uint16_t)(k->h + extra);
                given += extra;
                lastGrow = k;
            }
        }
        if (lastGrow && given < leftover) {
            int rem = leftover - given;
            if (isRow) lastGrow->w = (uint16_t)(lastGrow->w + rem);
            else       lastGrow->h = (uint16_t)(lastGrow->h + rem);
        }
        leftover = 0;   // consumed by grow
    }

    // Main-axis start offset + spacing per justify (only meaningful when leftover>0).
    int spacing = s.gap;
    int startOff = 0;
    if (leftover > 0) {
        switch (s.justify) {
            case Justify::Start:        startOff = 0;            break;
            case Justify::Center:       startOff = leftover / 2; break;
            case Justify::End:          startOff = leftover;     break;
            case Justify::SpaceBetween:
                if (nKids > 1) spacing = s.gap + leftover / (nKids - 1);
                else           startOff = leftover / 2;
                break;
        }
    }

    int cursor = (isRow ? innerX : innerY) + startOff;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        // Cross-axis sizing/positioning.
        if (s.align == Align::Stretch) {
            if (isRow) k->h = (uint16_t)crossAvail;
            else       k->w = (uint16_t)crossAvail;
        }
        int crossSize = isRow ? k->h : k->w;
        int crossOff = 0;
        switch (s.align) {
            case Align::Start:   crossOff = 0; break;
            case Align::Center:  crossOff = (crossAvail - crossSize) / 2; break;
            case Align::End:     crossOff = crossAvail - crossSize; break;
            case Align::Stretch: crossOff = 0; break;
        }
        if (crossOff < 0) crossOff = 0;

        if (isRow) { k->x = (int16_t)cursor;            k->y = (int16_t)(innerY + crossOff); }
        else       { k->x = (int16_t)(innerX + crossOff); k->y = (int16_t)cursor; }

        cursor += (isRow ? k->w : k->h) + spacing;
        arrange(k);
    }
}

void layout(UiNode& root, uint16_t rootW, uint16_t rootH, const TextMetrics& tm,
            int16_t originX, int16_t originY) {
    measure(&root, tm);
    root.x = originX;
    root.y = originY;
    if (root.style.width  == SIZE_AUTO) root.w = rootW;
    if (root.style.height == SIZE_AUTO) root.h = rootH;
    arrange(&root);
}

} // namespace nema::ui
