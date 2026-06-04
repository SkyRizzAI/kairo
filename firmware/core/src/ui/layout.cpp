#include "kairo/ui/layout.h"

namespace kairo::ui {

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

    // View / Pressable: measure children first.
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

    uint16_t contentMain  = (uint16_t)(sumMain + gaps + pad2);
    uint16_t contentCross = (uint16_t)(maxCross + pad2);

    uint16_t cw = isRow ? contentMain : contentCross;
    uint16_t ch = isRow ? contentCross : contentMain;

    n->w = (s.width  == SIZE_AUTO) ? cw : s.width;
    n->h = (s.height == SIZE_AUTO) ? ch : s.height;
}

// ── Pass 2: arrange (top-down); node x/y/w/h already final ─────────────────
static void arrange(UiNode* n) {
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

} // namespace kairo::ui
