#include "nema/skyrizze32/e32_key_map.h"
#include "nema/skyrizze32/board_config.h"

namespace nema::skyrizze32 {

using namespace input;

E32KeyMap::E32KeyMap() {
    engine_.setCallback(&E32KeyMap::onGesture, this);
    // Middle button = tap/double/hold → OK / Back / Pause (board-defined gesture
    // profile, Plan 22/37). Single click = OK, double click = Back, long-hold =
    // app pause. No repeat on the middle button.
    engine_.setDoubleHold(BTN_MIDDLE);
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
        case Action::Prev:       return "Left";     // below left (SW1)
        case Action::Next:       return "Right";    // below right (SW3)
        case Action::Activate:   return "OK";       // center (SW2), single tap
        case Action::Back:       return "2x OK";    // center (SW2), double tap
        case Action::Pause:      return "Hold OK";  // center (SW2), long-hold
        case Action::AdjustUp:   return "Up";       // side top (PB1)
        case Action::AdjustDown: return "Dn";       // side bottom (PB2)
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
        case BTN_MIDDLE:
            if (g == Gesture::Hold)   return Code::None;     // pause — no code
            if (g == Gesture::Double) return Code::Escape;   // back
            return Code::Enter;                              // tap = OK
        default:         return Code::None;
    }
}

// static
Action E32KeyMap::idToAction(uint8_t id, Gesture g) {
    switch (id) {
        // Below-left / below-right = primary nav (horizontal carousel).
        case BTN_LEFT:   return Action::Prev;        // Left
        case BTN_RIGHT:  return Action::Next;        // Right
        // Side buttons = secondary adjust (up/down).
        case BTN_UP:     return Action::AdjustUp;    // Up
        case BTN_DOWN:   return Action::AdjustDown;  // Down
        case BTN_MIDDLE:
            // Board gesture profile: tap = OK, double = Back, long-hold = Pause.
            if (g == Gesture::Hold)   return Action::Pause;
            if (g == Gesture::Double) return Action::Back;
            if (g == Gesture::Short)  return Action::Activate;
            return Action::None;
        default:
            return Action::None;
    }
}

} // namespace nema::skyrizze32
