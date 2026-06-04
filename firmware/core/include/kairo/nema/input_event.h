#pragma once
#include "kairo/ui/key.h"
#include "kairo/input/input_code.h"
#include "kairo/input/input_action.h"
#include "kairo/input/gesture.h"
#include <cstdint>

namespace kairo {

// A single input occurrence, produced by an input source and consumed by
// the main task / GUI. POD so it can flow through any queue cheaply.
//
// Backward compat: `key` is always populated — DPM and legacy screens use it.
// New screens should consume `action` via IScreen::onAction().
struct InputEvent {
    enum class Type : uint8_t { Press, Release, Repeat };

    Key  key  = Key::None;   // always set — backward compat
    Type type = Type::Press;

    // Extended fields (Plan 27). Set by IKeyMap or auto-filled by InputService.
    input::Code    code    = input::Code::None;
    input::Action  action  = input::Action::None;
    input::Gesture gesture = input::Gesture::Short;
};

} // namespace kairo
