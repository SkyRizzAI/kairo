#pragma once
#include "nema/input/i_key_map.h"
#include "nema/input/gesture.h"

namespace nema::skyrizze32 {

// E32KeyMap — 3-button IKeyMap for SkyRizz E32.
//
// Button layout (physical Left / Middle / Right):
//   BTN_LEFT   (SW1)  short → Prev       long → AdjustDown
//   BTN_MIDDLE (PB1)  short → Activate   long → Back
//   BTN_RIGHT  (SW2)  short → Next       long → AdjustUp
//
// Side buttons (PB1 Up / PB2 Down) → AdjustUp / AdjustDown.
// Floor guarantee: Prev, Next, Activate, Back — all reachable.
// No native Up/Down/Left/Right codes → "input.2d" capability NOT declared.
class E32KeyMap : public input::IKeyMap {
public:
    E32KeyMap();   // sets up GestureEngine callback

    void feedEdge(uint8_t buttonId, bool pressed, uint64_t nowMs) override;
    void tick(uint64_t nowMs) override;

    const char* boardName()   const override { return "skyrizz-e32"; }
    int         buttonCount() const override { return 5; }
    const char* buttonLabel(uint8_t id) const override;
    const char* hintFor(input::Action a) const override;
    bool        hasCode(input::Code c)   const override;
    bool        canReach(input::Action a) const override;

    // Gesture timing (set from Config Store at board init).
    void setLongMs(uint32_t ms)   { engine_.longMs   = ms; }
    void setRepeatMs(uint32_t ms) { engine_.repeatMs  = ms; }

private:
    input::GestureEngine engine_;

    static void onGesture(void* ctx, uint8_t id, input::Gesture g, uint64_t now);

    static input::Code   idToCode  (uint8_t id, input::Gesture g);
    static input::Action idToAction(uint8_t id, input::Gesture g);
};

} // namespace nema::skyrizze32
