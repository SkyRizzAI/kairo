#pragma once
#include <cstdint>

namespace nema::input {

// A pointer (touch) sample in LOGICAL canvas coordinates. The board's
// ITouchDriver is responsible for transforming raw panel coordinates (chip
// protocol, orientation, scale, resistive calibration) into this space, so
// components are 100% touch-controller-agnostic.
enum class PointerPhase : uint8_t { Down, Move, Up };

struct PointerEvent {
    PointerPhase phase = PointerPhase::Down;
    uint16_t     x = 0;
    uint16_t     y = 0;
};

// Which input modality was used last. Drives "focus-visible" behaviour: the
// focus ring is rendered only while navigating with buttons, and hidden once
// the screen is touched (like the web :focus-visible pseudo-class).
enum class InputModality : uint8_t { Button, Pointer };

} // namespace nema::input
