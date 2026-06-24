#include "nema/ui/virtual_list.h"
#include "nema/ui/widgets.h"
#include <algorithm>
#include <cstring>

namespace aether::ui {

static constexpr int OVERSCAN = 3;  // extra items rendered above/below viewport

UiNode* VirtualList(NodeArena& a, VirtualListState& vst,
                    int totalCount, uint16_t itemHeight,
                    UiNode* (*renderItem)(NodeArena& a, int index,
                                         bool focused, void* userdata),
                    void* userdata, Style style) {
    // Update cached metrics so moveFocus/scrollToFocused work correctly.
    vst.totalCount = totalCount;
    vst.itemH      = itemHeight;
    vst.clampFocus();

    // Viewport from last-frame layout measurement (0 on very first frame).
    // If unknown, render conservatively large to avoid a blank first frame.
    int vp = (int)vst.viewportMain;
    if (vp <= 0) vp = 9999;

    // Compute the rendered window: visible range + overscan.
    int firstVis = (itemHeight > 0) ? (int)vst.scrollMain / (int)itemHeight : 0;
    int lastVis  = (itemHeight > 0) ? firstVis + vp / (int)itemHeight     : totalCount - 1;
    int first    = std::max(0, firstVis - OVERSCAN);
    int last     = std::min(totalCount - 1, lastVis + OVERSCAN);

    // ── Scroll container ──────────────────────────────────────────────────────
    UiNode* scroll = a.alloc();
    if (!scroll) return nullptr;
    scroll->type        = NodeType::Scroll;
    scroll->scroll      = &vst;
    scroll->style       = style;
    scroll->style.dir      = FlexDir::Col;
    scroll->style.flexGrow = (style.flexGrow > 0) ? style.flexGrow : 1;

    // Walk tail pointer to build the child linked list in order.
    UiNode** tail = &scroll->firstChild;
    auto append = [&](UiNode* n) {
        if (!n) return;
        n->nextSibling = nullptr;
        *tail = n;
        tail  = &n->nextSibling;
    };

    // ── Top spacer ────────────────────────────────────────────────────────────
    // Represents items [0 .. first−1] that are off-screen above.
    // Its height makes the scrollbar thumb position correct.
    if (first > 0 && itemHeight > 0) {
        UiNode* sp = a.alloc();
        if (sp) {
            sp->style.height = (uint16_t)(first * (int)itemHeight);
            append(sp);
        }
    }

    // ── Rendered items ────────────────────────────────────────────────────────
    for (int i = first; i <= last && i < totalCount; ++i) {
        UiNode* item = renderItem(a, i, (i == vst.focusedIndex), userdata);
        if (!item) {
            // Caller returned nullptr → preserve height with an invisible spacer.
            item = a.alloc();
            if (item) item->style.height = itemHeight;
        }
        // Do NOT set focusable=true here. VirtualList keeps its internal nodes
        // out of ComponentRuntime's focus tree — navigation is via moveFocus().
        if (item) item->focusable = false;
        append(item);
    }

    // ── Bottom spacer ─────────────────────────────────────────────────────────
    int after = (totalCount > 0) ? (totalCount - 1 - last) : 0;
    if (after > 0 && itemHeight > 0) {
        UiNode* sp = a.alloc();
        if (sp) {
            sp->style.height = (uint16_t)(after * (int)itemHeight);
            append(sp);
        }
    }

    return scroll;
}

} // namespace aether::ui
