#pragma once
#include "nema/ui/node.h"

namespace nema {
class Canvas;
}

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

// Paint a laid-out tree to the Canvas. Call layout() first (with roleMetrics()).
// `focused` (optional) highlights that Pressable node with an inverted box.
void render(const UiNode& root, Canvas& c, const UiNode* focused = nullptr);

// Plan 52 — set the current frame timestamp (ms) before calling render().
// TextRole::Smart nodes use it for marquee scroll animation.
// GuiService calls this once per frame from the UI clock.
void setRenderTick(uint32_t ms);
uint32_t renderTick();

} // namespace aether::ui
