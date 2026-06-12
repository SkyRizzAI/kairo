#pragma once
#include "nema/ui/node.h"
#include <cstdint>

namespace nema::ui {

// Text measurement is injected so the layout engine stays pure logic (no Canvas
// / display dependency) and is host-unit-testable. The renderer provides a
// Canvas-backed measurer in production; tests provide a fake one.
struct TextMetrics {
    uint16_t (*width)(void* ctx, const char* text, TextRole role);
    uint16_t (*height)(void* ctx, TextRole role);
    void* ctx = nullptr;
};

// Two-pass flexbox-subset layout. Fills x/y/w/h (absolute logical px) on every
// node in the tree. (rootW, rootH) is the available logical area.
//
// Supported: row/col single main-axis, flexGrow weights, fixed width/height,
// uniform padding, gap, alignItems (Start/Center/End/Stretch), justifyContent
// (Start/Center/End/SpaceBetween). NOT supported: wrap, grid, absolute, margin,
// percentage sizing.
void layout(UiNode& root, uint16_t rootW, uint16_t rootH, const TextMetrics& tm,
            int16_t originX = 0, int16_t originY = 0);

} // namespace nema::ui
