#pragma once
#include "kairo/ui/node.h"

namespace kairo {
class Canvas;
}

namespace kairo::ui {

// Paint a laid-out tree to the Canvas. Call layout() first (with roleMetrics()).
// `focused` (optional) highlights that Pressable node with an inverted box.
void render(const UiNode& root, Canvas& c, const UiNode* focused = nullptr);

} // namespace kairo::ui
