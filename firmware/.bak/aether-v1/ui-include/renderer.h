#pragma once
#include "nema/ui/node.h"

namespace nema {
class Canvas;
}

namespace nema::ui {

// Paint a laid-out tree to the Canvas. Call layout() first (with roleMetrics()).
// `focused` (optional) highlights that Pressable node with an inverted box.
void render(const UiNode& root, Canvas& c, const UiNode* focused = nullptr);

} // namespace nema::ui
