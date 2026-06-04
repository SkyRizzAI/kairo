#include "kairo/skyrizze32/e32_key_map.h"
#include "kairo/skyrizze32/board_config.h"

namespace kairo::skyrizze32 {

using namespace input;

E32KeyMap::E32KeyMap() {
    engine_.setCallback(&E32KeyMap::onGesture, this);
}

void E32KeyMap::feedEdge(uint8_t buttonId, bool pressed, uint64_t nowMs) {
    if (buttonId >= 3) return;
    engine_.feedEdge(buttonId, pressed, nowMs);
}

void E32KeyMap::tick(uint64_t nowMs) {
    engine_.tick(nowMs);
}

// static
void E32KeyMap::onGesture(void* ctx, uint8_t id, Gesture g, uint64_t now) {
    auto* self = static_cast<E32KeyMap*>(ctx);
    Code   c = idToCode(id, g);
    Action a = idToAction(id, g);
    if (a != Action::None) self->emitEvent(c, a, g, now);
}

const char* E32KeyMap::buttonLabel(uint8_t id) const {
    switch (id) {
        case BTN_LEFT:   return "Left";
        case BTN_MIDDLE: return "Middle";
        case BTN_RIGHT:  return "Right";
        default:         return "?";
    }
}

const char* E32KeyMap::hintFor(Action a) const {
    switch (a) {
        case Action::Prev:       return "<";
        case Action::Next:       return ">";
        case Action::Activate:   return "[OK]";
        case Action::Back:       return "Hold [OK]";
        case Action::AdjustUp:   return "Hold >";
        case Action::AdjustDown: return "Hold <";
        default:                 return "";
    }
}

bool E32KeyMap::hasCode(Code c) const {
    // 3-button board has no native directional codes
    return c == Code::Enter || c == Code::Escape;
}

bool E32KeyMap::canReach(Action a) const {
    switch (a) {
        case Action::Prev:
        case Action::Next:
        case Action::Activate:
        case Action::Back:
        case Action::AdjustUp:
        case Action::AdjustDown: return true;
        default:                  return false;
    }
}

// static
Code E32KeyMap::idToCode(uint8_t id, Gesture g) {
    (void)g;
    switch (id) {
        case BTN_LEFT:   return Code::Left;
        case BTN_MIDDLE: return Code::Enter;
        case BTN_RIGHT:  return Code::Right;
        default:         return Code::None;
    }
}

// static
Action E32KeyMap::idToAction(uint8_t id, Gesture g) {
    switch (id) {
        case BTN_LEFT:
            return (g == Gesture::Long || g == Gesture::Repeat)
                ? Action::AdjustDown : Action::Prev;
        case BTN_MIDDLE:
            return (g == Gesture::Long)
                ? Action::Back : Action::Activate;
        case BTN_RIGHT:
            return (g == Gesture::Long || g == Gesture::Repeat)
                ? Action::AdjustUp : Action::Next;
        default:
            return Action::None;
    }
}

} // namespace kairo::skyrizze32
