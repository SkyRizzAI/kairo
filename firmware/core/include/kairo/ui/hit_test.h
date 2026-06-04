#pragma once
#include "kairo/ui/node.h"
#include <cstdint>

namespace kairo::ui {

// Return the TOP-MOST focusable+pressable node whose laid-out rect contains
// (x,y), or nullptr. "Top-most" = deepest in tree order (children paint over
// parents). Call layout() first so node rects are valid. Coordinates are in the
// same logical space the layout used (full-screen logical here).
UiNode* hitTest(UiNode& root, int16_t x, int16_t y);

} // namespace kairo::ui
