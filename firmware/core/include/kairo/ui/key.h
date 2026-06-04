#pragma once
#include <cstdint>
#include <cstring>

namespace kairo {

// The Kairo hardware has exactly 6 buttons. This enum is the single shared
// contract used by every input source (simulator stdin, ESP32 TCA9534 expander)
// and consumed uniformly by ViewDispatcher / IScreen::update().
//
// Hardware reference (OnionDAO badge, TCA9534 @ 0x20, active-LOW):
//   bit0 = Left   bit1 = Down   bit2 = Up
//   bit3 = Right  bit4 = Select bit5 = Cancel
enum class Key : uint8_t {
    None = 0,
    Up,
    Down,
    Left,
    Right,
    Select,   // "OK" / confirm
    Cancel,   // "back" / escape
};

inline const char* keyName(Key k) {
    switch (k) {
        case Key::Up:     return "Up";
        case Key::Down:   return "Down";
        case Key::Left:   return "Left";
        case Key::Right:  return "Right";
        case Key::Select: return "Select";
        case Key::Cancel: return "Cancel";
        default:          return "None";
    }
}

inline Key keyFromName(const char* s) {
    if (!s) return Key::None;
    if (!std::strcmp(s, "Up"))     return Key::Up;
    if (!std::strcmp(s, "Down"))   return Key::Down;
    if (!std::strcmp(s, "Left"))   return Key::Left;
    if (!std::strcmp(s, "Right"))  return Key::Right;
    if (!std::strcmp(s, "Select")) return Key::Select;
    if (!std::strcmp(s, "Cancel")) return Key::Cancel;
    return Key::None;
}

} // namespace kairo
