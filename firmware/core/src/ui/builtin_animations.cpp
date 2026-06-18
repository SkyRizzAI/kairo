// Plan 70 — Built-in UI animations.
// Spinner: 4-frame rotating dots animation (8x8 px, 2 fps).
#include "nema/ui/animation.h"

namespace nema::anim {

// 4 frames of a spinning indicator — each frame lights one quadrant:
// ● ○    ○ ●    ○ ○    ○ ○
// ○ ○    ○ ○    ● ○    ○ ●
static const uint8_t SPINNER_FRAME0[] = {
    0b11111000,
    0b10001000,
    0b10001000,
    0b10001000,
    0b11111000,
    0b00000000,
    0b00000000,
    0b00000000,
};
static const uint8_t SPINNER_FRAME1[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b11111000,
    0b10001000,
    0b10001000,
    0b10001000,
    0b11111000,
};
static const uint8_t SPINNER_FRAME2[] = {
    0b00011111,
    0b00010001,
    0b00010001,
    0b00010001,
    0b00011111,
    0b00000000,
    0b00000000,
    0b00000000,
};
static const uint8_t SPINNER_FRAME3[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00011111,
    0b00010001,
    0b00010001,
    0b00010001,
    0b00011111,
};

const AnimationFrame SPINNER_FRAMES[4] = {
    { SPINNER_FRAME0, 8, 8 },
    { SPINNER_FRAME1, 8, 8 },
    { SPINNER_FRAME2, 8, 8 },
    { SPINNER_FRAME3, 8, 8 },
};

Animation A_SPINNER = {
    SPINNER_FRAMES, 4,   // 4 frames
    2,                    // 2 fps (gentle animation)
    true,                 // loop
};

} // namespace nema::anim
