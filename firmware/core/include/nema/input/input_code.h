#pragma once
#include "nema/ui/key.h"
#include <cstdint>

namespace nema::input {

// Raw input code — geometric / physical-ish identity of a control signal.
// These may be absent on a given board (e.g. no Up/Down on a 3-button device).
// App code should prefer InputAction for navigation; use Code only when the
// specific physical identity matters (raw games, custom controls).
enum class Code : uint8_t {
    None   = 0,
    Up     = 1,
    Down   = 2,
    Left   = 3,
    Right  = 4,
    Enter  = 10,   // confirm / select
    Escape = 11,   // back / cancel
    Menu   = 12,   // context / hamburger
    // Custom codes registered by board/app: id >= 0x80 (see InputService::registerCode)
};

inline const char* codeName(Code c) {
    switch (c) {
        case Code::Up:     return "Up";
        case Code::Down:   return "Down";
        case Code::Left:   return "Left";
        case Code::Right:  return "Right";
        case Code::Enter:  return "Enter";
        case Code::Escape: return "Escape";
        case Code::Menu:   return "Menu";
        default:           return "None";
    }
}

// Map legacy Key enum → Code (backward compat).
inline Code codeFromKey(nema::Key k) {
    switch (k) {
        case nema::Key::Up:     return Code::Up;
        case nema::Key::Down:   return Code::Down;
        case nema::Key::Left:   return Code::Left;
        case nema::Key::Right:  return Code::Right;
        case nema::Key::Select: return Code::Enter;
        case nema::Key::Cancel: return Code::Escape;
        default:                 return Code::None;
    }
}

// Map Code → legacy Key (for backward compat with screens using update(Key)).
inline nema::Key keyFromCode(Code c) {
    switch (c) {
        case Code::Up:     return nema::Key::Up;
        case Code::Down:   return nema::Key::Down;
        case Code::Left:   return nema::Key::Left;
        case Code::Right:  return nema::Key::Right;
        case Code::Enter:  return nema::Key::Select;
        case Code::Escape: return nema::Key::Cancel;
        default:           return nema::Key::None;
    }
}

} // namespace nema::input
