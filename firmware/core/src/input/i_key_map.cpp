#include "kairo/input/i_key_map.h"
#include "kairo/services/input_service.h"
#include "kairo/nema/input_event.h"

namespace kairo::input {

bool IKeyMap::validateFloor() const {
    return canReach(Action::Prev)
        && canReach(Action::Next)
        && canReach(Action::Activate)
        && canReach(Action::Back);
}

void IKeyMap::emitEvent(Code code, Action action, Gesture gesture, uint64_t /*nowMs*/) {
    if (!input_) return;
    InputEvent e;
    e.code    = code;
    e.action  = action;
    e.gesture = gesture;
    e.key     = keyFromCode(code);          // backward compat for DPM / legacy screens
    e.type    = (gesture == Gesture::Repeat)
                ? InputEvent::Type::Repeat
                : InputEvent::Type::Press;
    input_->post(e);
}

} // namespace kairo::input
