#include "kairo/skyrizze32/e32_key_map.h"
#include "kairo/skyrizze32/board_config.h"

namespace kairo::skyrizze32 {

using namespace input;

E32KeyMap::E32KeyMap() {
    engine_.setCallback(&E32KeyMap::onGesture, this);
}

void E32KeyMap::feedEdge(uint8_t buttonId, bool pressed, uint64_t nowMs) {
    if (buttonId >= 5) return;   // SW1/SW2/SW3 + PB1/PB2
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
        case BTN_MIDDLE: return "OK";
        case BTN_RIGHT:  return "Right";
        case BTN_UP:     return "Up";
        case BTN_DOWN:   return "Down";
        default:         return "?";
    }
}

const char* E32KeyMap::hintFor(Action a) const {
    switch (a) {
        case Action::Prev:       return "Up";       // side top (PB1)
        case Action::Next:       return "Dn";       // side bottom (PB2)
        case Action::Activate:   return "OK";       // center (SW2), short
        case Action::Back:       return "Hold OK";  // center (SW2), hold
        case Action::AdjustUp:   return "Right";    // below right (SW3)
        case Action::AdjustDown: return "Left";     // below left (SW1)
        default:                 return "";
    }
}

bool E32KeyMap::hasCode(Code c) const {
    // All six directional/semantic codes are producible via short/long press.
    switch (c) {
        case Code::Up:
        case Code::Down:
        case Code::Left:
        case Code::Right:
        case Code::Enter:
        case Code::Escape: return true;
        default:           return false;
    }
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
    switch (id) {
        // Arrows: any tap/hold gesture → that arrow code (hold = repeat).
        case BTN_LEFT:   return Code::Left;
        case BTN_RIGHT:  return Code::Right;
        case BTN_UP:     return Code::Up;
        case BTN_DOWN:   return Code::Down;
        case BTN_MIDDLE: return (g == Gesture::Long) ? Code::Escape : Code::Enter;
        default:         return Code::None;
    }
}

// static
Action E32KeyMap::idToAction(uint8_t id, Gesture g) {
    switch (id) {
        // Below-left / below-right = horizontal arrows. Tap or hold-repeat.
        case BTN_LEFT:   return Action::AdjustDown;  // Left
        case BTN_RIGHT:  return Action::AdjustUp;    // Right
        // Side buttons = vertical nav (list up/down). Tap or hold-repeat.
        case BTN_UP:     return Action::Prev;        // Up
        case BTN_DOWN:   return Action::Next;        // Down
        case BTN_MIDDLE:
            // One-shot only: Short = OK, Long = Back. Ignore Repeat so holding
            // past the threshold doesn't fire Back-then-spam-Activate.
            if (g == Gesture::Long)  return Action::Back;
            if (g == Gesture::Short) return Action::Activate;
            return Action::None;
        default:
            return Action::None;
    }
}

} // namespace kairo::skyrizze32
