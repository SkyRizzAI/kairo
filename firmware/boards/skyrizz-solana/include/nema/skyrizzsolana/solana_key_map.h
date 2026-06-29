#pragma once
#include "nema/input/i_key_map.h"
#include "nema/input/gesture.h"

namespace nema::skyrizzsolana {

// SolanaKeyMap — 6-button IKeyMap for SkyRizz Solana.
//
// Physical layout: a 4-way D-pad + dedicated OK and Back buttons. Because Back
// has its own key, OK is free to carry tap = Activate / long-hold = Menu.
//   BTN_UP     → Prev        (navigate up / previous)
//   BTN_DOWN   → Next        (navigate down / next)
//   BTN_LEFT   → AdjustDown  (value down / left)
//   BTN_RIGHT  → AdjustUp    (value up / right)
//   BTN_OK     → Activate (tap) / Menu (long-hold)
//   BTN_BACK   → Back
//
// Floor guarantee: Prev, Next, Activate, Back — all reachable. Four arrows →
// full 2D directional input (grid virtual keyboard).
class SolanaKeyMap : public input::IKeyMap {
public:
    SolanaKeyMap();   // sets up GestureEngine callback

    void feedEdge(uint8_t buttonId, bool pressed, uint64_t nowMs) override;
    void tick(uint64_t nowMs) override;

    const char* boardName()   const override { return "skyrizz-solana"; }
    int         buttonCount() const override { return 6; }
    const char* buttonLabel(uint8_t id) const override;
    const char* hintFor(input::Action a) const override;
    bool        hasCode(input::Code c)   const override;
    bool        canReach(input::Action a) const override;

    // Remap the 4 directional buttons to match display rotation (Plan 92 Fase A).
    void        setRotation(uint8_t r) override { rotation_ = (uint8_t)(r & 3); }

    // Gesture timing (set from Config Store at board init).
    void setLongMs(uint32_t ms)   { engine_.longMs   = ms; }
    void setRepeatMs(uint32_t ms) { engine_.repeatMs = ms; }

private:
    input::GestureEngine engine_;
    uint8_t              rotation_ = 0;   // 0/1/2/3 → 0°/90°/180°/270°

    static void onGesture(void* ctx, uint8_t id, input::Gesture g, uint64_t now);

    // Map a physical directional button id to the one that matches the current
    // rotation (OK/Back are orientation-independent). Plan 92 Fase A.
    uint8_t rotateId(uint8_t id) const;

    static input::Code   idToCode  (uint8_t id, input::Gesture g);
    static input::Action idToAction(uint8_t id, input::Gesture g);
};

} // namespace nema::skyrizzsolana
