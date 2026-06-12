#pragma once
#include "nema/ui/key.h"
#include "nema/input/input_code.h"
#include "nema/input/input_action.h"
#include "nema/input/gesture.h"
#include "nema/input/pointer.h"
#include <cstdint>

namespace nema {

// A single input occurrence, produced by an input source and consumed by
// the main task / GUI. POD so it can flow through any queue cheaply.
//
// ONE funnel carries two kinds (Plan 29):
//   Kind::Key     — button/d-pad: key + code + action + gesture
//   Kind::Pointer — touch: pphase + px/py (logical coords)
//
// Backward compat: `key` is always populated for Key events — DPM and legacy
// screens use it. New screens consume `action` (buttons) / pointer (touch).
struct InputEvent {
    enum class Type : uint8_t { Press, Release, Repeat };
    enum class Kind : uint8_t { Key, Pointer };

    Kind kind = Kind::Key;

    // ── Key payload (kind == Key) ─────────────────────────────────────────
    Key  key  = Key::None;   // always set for Key events — backward compat
    Type type = Type::Press;
    input::Code    code    = input::Code::None;
    input::Action  action  = input::Action::None;
    input::Gesture gesture = input::Gesture::Short;

    // ── Pointer payload (kind == Pointer) ────────────────────────────────
    input::PointerPhase pphase = input::PointerPhase::Down;
    uint16_t px = 0;
    uint16_t py = 0;
};

} // namespace nema
