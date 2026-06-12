#pragma once
#include <cstdint>
#include <cstring>

namespace nema::input {

// Gesture produced by the gesture engine from raw press/release edges.
enum class Gesture : uint8_t {
    Short  = 0,   // press + release < long_ms
    Long   = 1,   // press held >= long_ms (fired at release or at threshold)
    Double = 2,   // two Short presses within double_ms  (future)
    Chord  = 3,   // two buttons pressed within chord_ms (future, Ledger-style)
    Repeat = 4,   // held past long_ms, periodic re-fire at repeat_ms
    Hold   = 5,   // two-stage button held >= hold_ms (longer than Long) — e.g. pause
};

inline const char* gestureName(Gesture g) {
    switch (g) {
        case Gesture::Short:  return "Short";
        case Gesture::Long:   return "Long";
        case Gesture::Double: return "Double";
        case Gesture::Chord:  return "Chord";
        case Gesture::Repeat: return "Repeat";
        case Gesture::Hold:   return "Hold";
        default:              return "?";
    }
}

// Per-button state tracked by GestureEngine.
struct ButtonState {
    uint64_t pressTime   = 0;
    uint64_t lastRepeat  = 0;
    uint64_t lastTapTime = 0;       // double-tap window anchor
    bool     isPressed   = false;
    bool     longFired   = false;
    bool     holdFired   = false;   // two-stage/double: Hold already emitted this press
    bool     pendingShort = false;  // double mode: a tap awaiting the double window
    bool     consumed     = false;  // double mode: this press was a double's 2nd tap
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
    uint32_t longMs   = 500;    // hold >= this → Long
    uint32_t repeatMs = 150;    // repeat interval after Long
    uint32_t holdMs   = 1000;   // two-stage/double: hold >= this → Hold (e.g. pause)
    uint32_t doubleMs = 280;    // two taps within this → Double (else Short)

    using GestureCb = void(*)(void* ctx, uint8_t buttonId, Gesture g, uint64_t now);

    void setCallback(GestureCb cb, void* ctx) { cb_ = cb; ctx_ = ctx; }

    // Mark a button "two-stage": it does NOT repeat; instead a quick release =
    // Short, a medium hold-then-release = Long, and holding past holdMs = Hold.
    // Lets one button carry tap / hold / long-hold (e.g. Activate / Back / Pause).
    void setTwoStage(uint8_t id, bool on = true) { if (id < MAX_BUTTONS) twoStage_[id] = on; }

    // Mark a button "double+hold": single tap = Short (fired after the double
    // window), two quick taps = Double, hold past holdMs = Hold. No repeat. Lets
    // one button carry tap / double-tap / long-hold (e.g. OK / Back / Pause).
    void setDoubleHold(uint8_t id, bool on = true) { if (id < MAX_BUTTONS) doubleHold_[id] = on; }

    // Call from poll thread on button edge.
    void feedEdge(uint8_t id, bool pressed, uint64_t now);

    // Call every poll tick (15ms) for long/repeat detection.
    void tick(uint64_t now);

    void reset();

private:
    ButtonState states_[MAX_BUTTONS] = {};
    bool        twoStage_[MAX_BUTTONS] = {};
    bool        doubleHold_[MAX_BUTTONS] = {};
    GestureCb   cb_  = nullptr;
    void*       ctx_ = nullptr;

    void fire(uint8_t id, Gesture g, uint64_t now);
};

} // namespace nema::input
