#pragma once
#include "nema/ui/node.h"
#include <cstdint>

namespace nema::ui {

// True if (x,y) falls within the laid-out rect of n.
inline bool nodeContains(const UiNode* n, int16_t x, int16_t y) {
    return x >= n->x && x < n->x + (int)n->w &&
           y >= n->y && y < n->y + (int)n->h;
}

// Return the TOP-MOST focusable+pressable node whose laid-out rect contains
// (x,y), or nullptr. "Top-most" = deepest in tree order (children paint over
// parents). Call layout() first so node rects are valid. Coordinates are in the
// same logical space the layout used (full-screen logical here).
UiNode* hitTest(UiNode& root, int16_t x, int16_t y);

} // namespace nema::ui
