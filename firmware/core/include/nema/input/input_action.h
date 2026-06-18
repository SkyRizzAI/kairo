#pragma once
#include "nema/input/input_code.h"
#include "nema/ui/key.h"
#include <cstdint>

namespace nema::input {

// Navigation intent — hardware-agnostic.
//
// Every board MUST be able to produce the four floor actions:
//   Prev, Next, Activate, Back
// regardless of button count (via short/long/chord gestures). This is validated
// at board init by IKeyMap::validateFloor(). If validation fails, boot halts.
//
// Screen code should be written against Action, not Key or Code. The default
// IScreen::onAction() implementation forwards to the legacy update(Key) path
// so existing screens keep working without changes.
enum class Action : uint8_t {
    None = 0,

    // ── Floor (must always be reachable) ──────────────────────────────────
    Prev     = 1,   // navigate backward  (Left / Up / button-left)
    Next     = 2,   // navigate forward   (Right / Down / button-right)
    Activate = 3,   // confirm / enter
    Back     = 4,   // go back / escape

    // ── Optional (may be absent) ──────────────────────────────────────────
    AdjustUp   = 11,  // increment value  (Up / long-right)
    AdjustDown = 12,  // decrement value  (Down / long-left)
    Menu       = 13,  // context menu
    Pause      = 14,  // pause foreground app → home (long-hold Back/OK; Plan 22)
};

inline const char* actionName(Action a) {
    switch (a) {
        case Action::Prev:       return "Prev";
        case Action::Next:       return "Next";
        case Action::Activate:   return "Activate";
        case Action::Back:       return "Back";
        case Action::AdjustUp:   return "AdjustUp";
        case Action::AdjustDown: return "AdjustDown";
        case Action::Menu:       return "Menu";
        case Action::Pause:      return "Pause";
        default:                 return "None";
    }
}

// Default reduction: Code → Action. Boards may override in their IKeyMap.
// Primary nav axis = Left/Right; Up/Down = secondary (adjust).
inline Action defaultAction(Code c) {
    // (include input_code.h before this)
    switch (static_cast<uint8_t>(c)) {
        case 1:  return Action::AdjustUp;    // Up
        case 2:  return Action::AdjustDown;  // Down
        case 3:  return Action::Prev;        // Left
        case 4:  return Action::Next;        // Right
        case 10: return Action::Activate;    // Enter
        case 11: return Action::Back;        // Escape
        case 12: return Action::Menu;        // Menu
        default: return Action::None;
    }
}

// Map Action → legacy Key for backward-compat forward in IScreen::onAction().
inline nema::Key keyFromAction(Action a) {
    switch (a) {
        case Action::Prev:       return nema::Key::Left;
        case Action::Next:       return nema::Key::Right;
        case Action::Activate:   return nema::Key::Select;
        case Action::Back:       return nema::Key::Cancel;
        case Action::AdjustUp:   return nema::Key::Up;
        case Action::AdjustDown: return nema::Key::Down;
        default:                 return nema::Key::None;
    }
}

} // namespace nema::input
