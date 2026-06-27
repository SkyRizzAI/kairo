#include "nema/ui/layout.h"
#include "nema/ui/draw.h"
#include "nema/ui/ui_constants.h"

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

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
        if (s.widthScale < 1.0f) tw = (uint16_t)(tw * s.widthScale + 0.5f);  // collapse anim
        uint16_t th;
        // F2.3: multiline text measurement — use measured wrap height when width is fixed.
        // If wrap but width is auto, fall through to single-line (graceful degradation).
        if (n->wrap && s.width != SIZE_AUTO) {
            th = aether::ui::draw::measureMultilineH(n->text, s.width, n->role);
        } else {
            th = tm.height(tm.ctx, n->role);
        }
        n->w = (s.width  == SIZE_AUTO) ? (uint16_t)(tw + pad2) : s.width;
        n->h = (s.height == SIZE_AUTO) ? (uint16_t)(th + pad2) : s.height;
        // F2.2: apply min/max constraints
        if (s.minW > 0 && n->w < s.minW) n->w = s.minW;
        if (s.maxW != SIZE_AUTO && n->w > s.maxW) n->w = s.maxW;
        if (s.minH > 0 && n->h < s.minH) n->h = s.minH;
        if (s.maxH != SIZE_AUTO && n->h > s.maxH) n->h = s.maxH;
        return;
    }

    if (n->type == NodeType::Slider) {
        constexpr uint16_t SLIDER_H = 11;
        n->w = (s.width  == SIZE_AUTO) ? 0 : s.width;
        n->h = (s.height == SIZE_AUTO) ? SLIDER_H : s.height;
        // F2.2: apply min/max constraints
        if (s.minW > 0 && n->w < s.minW) n->w = s.minW;
        if (s.maxW != SIZE_AUTO && n->w > s.maxW) n->w = s.maxW;
        if (s.minH > 0 && n->h < s.minH) n->h = s.minH;
        if (s.maxH != SIZE_AUTO && n->h > s.maxH) n->h = s.maxH;
        return;
    }

    if (n->type == NodeType::Icon) {
        // Leaf: fixed size from the bitmap dimensions + padding.
        const uint16_t pad2 = (uint16_t)(s.padding * 2);
        const uint8_t  sc   = n->iconScale ? n->iconScale : 1;
        n->w = (s.width  == SIZE_AUTO) ? (uint16_t)(n->iconW * sc + pad2) : s.width;
        n->h = (s.height == SIZE_AUTO) ? (uint16_t)(n->iconH * sc + pad2) : s.height;
        // F2.2: apply min/max constraints
        if (s.minW > 0 && n->w < s.minW) n->w = s.minW;
        if (s.maxW != SIZE_AUTO && n->w > s.maxW) n->w = s.maxW;
        if (s.minH > 0 && n->h < s.minH) n->h = s.minH;
        if (s.maxH != SIZE_AUTO && n->h > s.maxH) n->h = s.maxH;
        return;
    }

    // View / Pressable / Scroll: measure children first.
    // F2.4: absolute children are measured (to get their natural size) but excluded
    // from sumMain/maxCross so they don't inflate the parent's content size.
    int n_children = 0;
    uint32_t sumMain = 0, maxCross = 0;
    const bool isRow = (s.dir == FlexDir::Row);
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        measure(k, tm);
        if (k->style.position == Position::Absolute) continue;
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
        // F2.2: apply min/max constraints
        if (s.minW > 0 && n->w < s.minW) n->w = s.minW;
        if (s.maxW != SIZE_AUTO && n->w > s.maxW) n->w = s.maxW;
        if (s.minH > 0 && n->h < s.minH) n->h = s.minH;
        if (s.maxH != SIZE_AUTO && n->h > s.maxH) n->h = s.maxH;
        return;
    }

    uint16_t contentMain  = (uint16_t)(sumMain + gaps + pad2);
    uint16_t cw = isRow ? contentMain : contentCross;
    uint16_t ch = isRow ? contentCross : contentMain;

    n->w = (s.width  == SIZE_AUTO) ? cw : s.width;
    n->h = (s.height == SIZE_AUTO) ? ch : s.height;
    // F2.2: apply min/max constraints
    if (s.minW > 0 && n->w < s.minW) n->w = s.minW;
    if (s.maxW != SIZE_AUTO && n->w > s.maxW) n->w = s.maxW;
    if (s.minH > 0 && n->h < s.minH) n->h = s.minH;
    if (s.maxH != SIZE_AUTO && n->h > s.maxH) n->h = s.maxH;
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

    // Responsive scrollbar gutter: when a vertical scrollbar is shown (content overflows),
    // reserve space on the cross axis so stretched rows stay clear of it — and reclaim that
    // space when the bar disappears. The dashed bar sits ~5px in from the container's right
    // edge; the inner box already drops `pad`, so reserve the remainder. This is why list
    // rows need NO per-row scrollbar gutter: the row itself narrows, so its focus fill and
    // right-aligned accessories (Switch/chevron) reposition automatically.
    const bool vbar = !isRow && n->scroll && contentMain > viewportMain;
    int reserve = vbar ? ((int)nema::display::SCROLLBAR_RESERVE - pad) : 0;
    if (reserve < 0) reserve = 0;
    const int crossUsable = crossAvail - reserve;

    // Lay children stacked at their natural main size, offset by the scroll pos.
    int cursor = (isRow ? innerX : innerY) - sc;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        // F2.4: skip absolute children in scroll flow
        if (k->style.position == Position::Absolute) continue;

        // F2.1: advance cursor by main-axis leading margin before placing child
        cursor += isRow ? k->style.ml : k->style.mt;

        if (s.align == Align::Stretch) {
            // Subtract the child's cross-axis margins so a stretched child with ml/mr (or
            // mt/mb on a Row scroll) is inset on BOTH sides instead of overflowing — the
            // position shift below adds ml/mt, so the width must drop ml+mr / mt+mb.
            if (isRow) {
                int hh = crossUsable - k->style.mt - k->style.mb;
                k->h = (uint16_t)(hh > 0 ? hh : 0);
            } else {
                int ww = crossUsable - k->style.ml - k->style.mr;
                k->w = (uint16_t)(ww > 0 ? ww : 0);
            }
        }
        // A wrapped Text now has its final (stretched) width → recompute its height so the
        // paragraph isn't clipped to one line. True <p>: width = container, height = auto.
        if (!isRow && k->type == NodeType::Text && k->wrap && k->text && k->w > 0) {
            uint16_t p2 = (uint16_t)(k->style.padding * 2);
            uint16_t iw = (k->w > p2) ? (uint16_t)(k->w - p2) : k->w;
            k->h = (uint16_t)(aether::ui::draw::measureMultilineH(k->text, iw, k->role) + p2);
        }
        int crossSize = isRow ? k->h : k->w;
        int crossOff = 0;
        switch (s.align) {
            case Align::Start:   crossOff = 0; break;
            case Align::Center:  crossOff = (crossUsable - crossSize) / 2; break;
            case Align::End:     crossOff = crossUsable - crossSize; break;
            case Align::Stretch: crossOff = 0; break;
        }
        if (crossOff < 0) crossOff = 0;

        if (isRow) { k->x = (int16_t)cursor;             k->y = (int16_t)(innerY + crossOff); }
        else       { k->x = (int16_t)(innerX + crossOff); k->y = (int16_t)cursor; }

        // F2.1: apply per-child margins to final position
        k->x += k->style.ml;
        k->y += k->style.mt;

        cursor += (isRow ? k->w : k->h) + s.gap;
        // F2.1: advance cursor by main-axis trailing margin after placing child
        cursor += isRow ? k->style.mr : k->style.mb;
        arrange(k);
    }

    // Re-derive content length from the ACTUAL laid-out children — heights may have grown
    // (e.g. wrapped paragraphs) since measure(), so the scrollbar + clamp see the true size.
    if (n->scroll) {
        int startMain = (isRow ? innerX : innerY) - sc;
        int extent = cursor - startMain - (int)s.gap;   // minus the trailing gap
        n->scroll->contentMain = (uint16_t)(extent > 0 ? extent : 0);
    }

    // F2.4: place absolute children pinned to parent origin after relative flow
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        if (k->style.position != Position::Absolute) continue;
        k->x = (int16_t)(n->x + k->style.absX);
        k->y = (int16_t)(n->y + k->style.absY);
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

    if (!n->firstChild) return;

    // F2.4: count only relative children for flex flow; absolute children are
    // placed separately after the flex pass and do not affect sizing/spacing.
    int nKids = 0;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        if (k->style.position != Position::Absolute) nKids++;
    }

    // flex-basis:0 — grow children that opt in contribute 0 to the base sum, so
    // the leftover (and thus the ratio split) ignores their content width.
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        if (k->style.position == Position::Absolute) continue;
        if (k->style.flexZero && k->style.flexGrow > 0) {
            if (isRow) k->w = 0; else k->h = 0;
        }
    }

    // Sum measured main sizes + collect grow weights.
    int sumMain = 0, growWeight = 0;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        if (k->style.position == Position::Absolute) continue;
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
            if (k->style.position == Position::Absolute) continue;
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
        // F2.4: skip absolute children in flex flow
        if (k->style.position == Position::Absolute) continue;

        // F2.1: advance cursor by main-axis leading margin before placing child
        cursor += isRow ? k->style.ml : k->style.mt;

        // Cross-axis sizing/positioning. Subtract the child's cross-axis margins so a
        // stretched child with margins is inset on both sides (consistent with
        // arrangeScroll) instead of overflowing when its position is shifted by ml/mt.
        if (s.align == Align::Stretch) {
            if (isRow) { int hh = crossAvail - k->style.mt - k->style.mb; k->h = (uint16_t)(hh > 0 ? hh : 0); }
            else       { int ww = crossAvail - k->style.ml - k->style.mr; k->w = (uint16_t)(ww > 0 ? ww : 0); }
        }
        // Wrapped Text: recompute height from the final stretched width (true <p>).
        if (!isRow && k->type == NodeType::Text && k->wrap && k->text && k->w > 0) {
            uint16_t p2 = (uint16_t)(k->style.padding * 2);
            uint16_t iw = (k->w > p2) ? (uint16_t)(k->w - p2) : k->w;
            k->h = (uint16_t)(aether::ui::draw::measureMultilineH(k->text, iw, k->role) + p2);
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

        // F2.1: apply per-child margins to final position
        k->x += k->style.ml;
        k->y += k->style.mt;

        cursor += (isRow ? k->w : k->h) + spacing;
        // F2.1: advance cursor by main-axis trailing margin after placing child
        cursor += isRow ? k->style.mr : k->style.mb;
        arrange(k);
    }

    // F2.4: place absolute children pinned to parent origin after relative flow
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        if (k->style.position != Position::Absolute) continue;
        k->x = (int16_t)(n->x + k->style.absX);
        k->y = (int16_t)(n->y + k->style.absY);
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

} // namespace aether::ui
