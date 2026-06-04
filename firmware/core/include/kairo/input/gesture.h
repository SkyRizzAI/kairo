#pragma once
#include <cstdint>
#include <cstring>

namespace kairo::input {

// Gesture produced by the gesture engine from raw press/release edges.
enum class Gesture : uint8_t {
    Short  = 0,   // press + release < long_ms
    Long   = 1,   // press held >= long_ms (fired at release or at threshold)
    Double = 2,   // two Short presses within double_ms  (future)
    Chord  = 3,   // two buttons pressed within chord_ms (future, Ledger-style)
    Repeat = 4,   // held past long_ms, periodic re-fire at repeat_ms
};

inline const char* gestureName(Gesture g) {
    switch (g) {
        case Gesture::Short:  return "Short";
        case Gesture::Long:   return "Long";
        case Gesture::Double: return "Double";
        case Gesture::Chord:  return "Chord";
        case Gesture::Repeat: return "Repeat";
        default:              return "?";
    }
}

// Per-button state tracked by GestureEngine.
struct ButtonState {
    uint64_t pressTime   = 0;
    uint64_t lastRepeat  = 0;
    bool     isPressed   = false;
    bool     longFired   = false;
};

// Gesture engine — converts raw press/release edges into Gesture events.
// Lives inside each IKeyMap implementation; tick() is called by the poll loop.
//
// Supported: Short, Long, Repeat.
// Planned (not yet): Double, Chord (see plan 27 non-goals v1).
class GestureEngine {
public:
    static constexpr int MAX_BUTTONS = 8;

    // Timing params — updated at runtime from config store.
    uint32_t longMs   = 500;   // hold >= this → Long
    uint32_t repeatMs = 150;   // repeat interval after Long

    using GestureCb = void(*)(void* ctx, uint8_t buttonId, Gesture g, uint64_t now);

    void setCallback(GestureCb cb, void* ctx) { cb_ = cb; ctx_ = ctx; }

    // Call from poll thread on button edge.
    void feedEdge(uint8_t id, bool pressed, uint64_t now);

    // Call every poll tick (15ms) for long/repeat detection.
    void tick(uint64_t now);

    void reset();

private:
    ButtonState states_[MAX_BUTTONS] = {};
    GestureCb   cb_  = nullptr;
    void*       ctx_ = nullptr;

    void fire(uint8_t id, Gesture g, uint64_t now);
};

} // namespace kairo::input
