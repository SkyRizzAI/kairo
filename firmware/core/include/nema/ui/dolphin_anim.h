#pragma once
#include "nema/ui/animation.h"
#include <cstddef>

namespace nema::anim {

// Plan 71 — Flipper Zero dolphin animation showcase.
// 10 animations, compiled-in. Navigate with Prev/Next.

constexpr size_t DOLPHIN_SHOWCASE_COUNT = 10;

extern Animation* const DOLPHIN_SHOWCASE[];

struct DolphinMeta {
    const char* name;
    uint16_t    w;
    uint16_t    h;
    uint8_t     fps;
    uint8_t     frames;
};
extern const DolphinMeta DOLPHIN_META[];

} // namespace nema::anim
