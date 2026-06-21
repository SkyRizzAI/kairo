#pragma once
#include <cstddef>
#include <cstdint>

namespace nema::anim {

// Plan 70: Frame-based animation data model.
//
// An Animation is a compiled sequence of 1‑bit XBM frames with timing metadata.
// Data stays in flash (constexpr-friendly); a separate AnimationPlayer holds the
// runtime state (current frame, playing flag, last‑tick timestamp).
//
// The AnimationManager (animation_manager.h) ticks all registered players from
// the GuiService loop so animations advance in lockstep with the render loop.

// Single frame — 1-bit XBM bitmap data.
struct AnimationFrame {
    const uint8_t* bitmap;     // raw XBM data (width * height / 8 bytes)
    uint16_t       width;
    uint16_t       height;
};

// Animation definition — sequence of frames + playback settings.
struct Animation {
    const AnimationFrame* frames;       // array of UNIQUE bitmaps
    uint8_t               frameCount;   // number of unique bitmaps
    uint8_t               frameRate;    // frames per second (0 = static)
    bool                  loop;         // restart after last playhead position

    // Optional non-linear playback (Flipper "Frames order").
    // null → sequential 0…frameCount-1 (backward-compatible default).
    const uint8_t*        framesOrder;    // playback index sequence
    uint8_t               framesOrderLen; // length of framesOrder[]
    uint8_t               passiveCount;   // leading passive (idle loop) frames
    uint8_t               activeCount;    // trailing active (triggered) frames
};

// Convenience macro for simple sequential animations (backward-compatible).
#define ANIM_DEF(name, rate, loop_flag, ...)                         \
    static const nema::anim::AnimationFrame name##_frames[] = {      \
        __VA_ARGS__                                                  \
    };                                                               \
    static const nema::anim::Animation name = {                      \
        name##_frames,                                               \
        (uint8_t)(sizeof(name##_frames) / sizeof(name##_frames[0])), \
        (rate), (loop_flag),                                         \
        nullptr, 0, 0, 0                                             \
    }

} // namespace nema::anim
