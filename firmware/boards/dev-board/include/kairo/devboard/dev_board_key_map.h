#pragma once
#include "kairo/input/i_key_map.h"

namespace kairo {

// DevBoardKeyMap — IKeyMap for the 6-button TCA9534 dev board.
//
// All 6 buttons are short-press only (no long/chord needed on this hardware).
// Mapping:
//   Left   → Code::Left   → Action::AdjustDown
//   Down   → Code::Down   → Action::Next
//   Up     → Code::Up     → Action::Prev
//   Right  → Code::Right  → Action::AdjustUp
//   Select → Code::Enter  → Action::Activate
//   Cancel → Code::Escape → Action::Back
class DevBoardKeyMap : public input::IKeyMap {
public:
    // button IDs match TCA9534 bit positions 0-5
    static constexpr uint8_t BTN_LEFT   = 0;
    static constexpr uint8_t BTN_DOWN   = 1;
    static constexpr uint8_t BTN_UP     = 2;
    static constexpr uint8_t BTN_RIGHT  = 3;
    static constexpr uint8_t BTN_SELECT = 4;
    static constexpr uint8_t BTN_CANCEL = 5;

    void feedEdge(uint8_t buttonId, bool pressed, uint64_t nowMs) override;
    void tick(uint64_t /*nowMs*/) override {}  // no long/repeat needed on 6-button board

    const char* boardName()   const override { return "dev-board"; }
    int         buttonCount() const override { return 6; }
    const char* buttonLabel(uint8_t id) const override;
    const char* hintFor(input::Action a) const override;
    bool        hasCode(input::Code c)   const override;
    bool        canReach(input::Action a) const override { (void)a; return true; }

private:
    static input::Code   idToCode(uint8_t id);
    static input::Action idToAction(uint8_t id);
};

} // namespace kairo
