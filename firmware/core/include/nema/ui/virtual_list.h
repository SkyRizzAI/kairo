#pragma once
#include "nema/ui/node.h"
#include "nema/ui/widgets.h"
#include <algorithm>
#include <cstdint>

namespace aether::ui {
using namespace nema;

// ── VirtualListState ─────────────────────────────────────────────────────────
//
// Persistent state for a VirtualList. Lives as a member of the screen/app
// (like ScrollState) so focus and scroll position survive per-frame rebuilds.
//
// Focus is managed by INDEX, not by tree pointer, so it remains stable when
// the rendered window shifts (items scroll in/out of view).
//
// Navigation pattern (in ComponentScreen::onAction / ComponentApp::onKey):
//
//   if (a == input::Action::Prev)     { if (list_.moveFocus(-1)) dirty_=true; }
//   if (a == input::Action::Next)     { if (list_.moveFocus(+1)) dirty_=true; }
//   if (a == input::Action::Activate) { handleSelect(list_.focusedIndex); }
//
struct VirtualListState : ScrollState {
    int      focusedIndex = 0;
    int      totalCount   = 0;
    uint16_t itemH        = 0;   // set by VirtualList() builder each frame

    // Move focus by dir (−1 = up, +1 = down).
    // Adjusts scrollMain to keep the focused item in view.
    // Returns true if focus changed (caller should redraw).
    bool moveFocus(int dir) {
        if (totalCount == 0) return false;
        int next = focusedIndex + dir;
        if (next < 0 || next >= totalCount) return false;
        focusedIndex = next;
        scrollToFocused();
        return true;
    }

    // Clamp focusedIndex into [0, totalCount−1] after count changes.
    void clampFocus() {
        if (totalCount == 0) { focusedIndex = 0; return; }
        focusedIndex = std::max(0, std::min(focusedIndex, totalCount - 1));
    }

    // Scroll so the focused item is fully inside the viewport.
    void scrollToFocused() {
        if (itemH == 0 || totalCount == 0) return;
        int top    = focusedIndex * (int)itemH;
        int bottom = top + (int)itemH;
        int vp     = (int)viewportMain;
        if (vp <= 0) return;
        if (top < (int)scrollMain)
            scrollMain = (int16_t)top;
        else if (bottom > (int)scrollMain + vp)
            scrollMain = (int16_t)(bottom - vp);
        int ms = (int)maxScroll();
        scrollMain = (int16_t)std::max(0, std::min((int)scrollMain, ms));
    }
};

// ── VirtualList builder ───────────────────────────────────────────────────────
//
// Creates a ScrollView that renders ONLY the visible window of items plus a
// small overscan buffer. Top and bottom spacer nodes give the scrollbar the
// correct thumb size without allocating hidden items.
//
// renderItem(arena, index, focused, userdata) → UiNode*
//   Called for each item in the rendered window. `focused` is true when
//   index == vst.focusedIndex — use it to apply a selectBox highlight.
//   Return nullptr to leave an empty slot (an invisible spacer is placed).
//
// Focus and input are ENTIRELY app-managed via VirtualListState::moveFocus().
// VirtualList does NOT hook into ComponentRuntime's focus tree — it keeps
// focusable=false on all internal nodes so the two systems don't interfere.
//
UiNode* VirtualList(NodeArena& a, VirtualListState& vst,
                    int totalCount, uint16_t itemHeight,
                    UiNode* (*renderItem)(NodeArena& a, int index,
                                         bool focused, void* userdata),
                    void* userdata,
                    Style style = {});

} // namespace aether::ui
