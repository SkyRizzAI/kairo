#pragma once
#include "nema/ui/node.h"
#include "nema/ui/layout.h"
#include "nema/ui/font_registry.h"
#include <cstdint>

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

// Resolves a TextRole to a concrete font handle + integer render scale.
// Layout (measure) and renderer (paint) BOTH go through here so sizes match.
//
// A global text-size preference (Normal/Large) shifts the mapping — see
// setTextSize(). Until multi-size fonts exist (Plan 25 Phase 3), larger roles
// use pixel-doubling (drawTextScaled) of FONT_5X8.
struct FontSpec {
    FontHandle handle;   // Plan 70: resolved via FontRegistry
    uint8_t    scale;    // 1 = normal, 2 = pixel-doubled, ...
};

enum class TextSize : uint8_t { Normal, Large };

void     setTextSize(TextSize sz);   // global; e.g. from config "display/text_size"
TextSize textSize();

FontSpec fontForRole(TextRole role);

// Logical-pixel measurement consistent with how the renderer paints.
uint16_t measureTextW(const char* s, TextRole role);
uint16_t measureTextH(TextRole role);

// A TextMetrics (for layout()) backed by the role table above. ctx unused.
TextMetrics roleMetrics();

} // namespace aether::ui
