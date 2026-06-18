#include "nema/services/input_service.h"
#include "nema/input/i_key_map.h"

namespace nema {

void InputService::post(Key k, InputEvent::Type t) {
    InputEvent e;
    e.key    = k;
    e.type   = t;
    e.code   = input::codeFromKey(k);
    e.action = input::defaultAction(e.code);
    e.gesture = input::Gesture::Short;
    queue_.send(e);
}

void InputService::setKeyMap(input::IKeyMap* km) {
    keyMap_ = km;
    if (km) km->attachInput(this);
}

const char* InputService::hintFor(input::Action a) const {
    if (keyMap_) return keyMap_->hintFor(a);
    // Fallback hints when no keymap (simulator / host)
    switch (a) {
        case input::Action::Prev:       return "Left";
        case input::Action::Next:       return "Right";
        case input::Action::Activate:   return "Select";
        case input::Action::Back:       return "Cancel";
        case input::Action::AdjustUp:   return "Up";
        case input::Action::AdjustDown: return "Down";
        default:                        return "";
    }
}

bool InputService::hasCode(input::Code c) const {
    if (keyMap_) return keyMap_->hasCode(c);
    // Without a keymap (simulator), assume all core codes available
    return c != input::Code::None;
}

bool InputService::canReach(input::Action a) const {
    if (keyMap_) return keyMap_->canReach(a);
    return a != input::Action::None;
}

bool InputService::validateFloor() const {
    if (keyMap_) return keyMap_->validateFloor();
    return true;  // simulator: all actions reachable
}

} // namespace nema
