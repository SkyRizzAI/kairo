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
    uint8_t eid = self->rotateId(id);    // follow display rotation (Plan 92 Fase A)
    Code   c = idToCode(eid, g);
    Action a = idToAction(eid, g);
    if (a != Action::None) self->emitEvent(c, a, g, now);
}

// Rotate the 4 directional buttons to match the display orientation so input
// feels natural after the device is physically turned. Ring order Up→Right→Down→
// Left; we step the OPPOSITE way (`-rotation_`) because the panel rotates CCW
// relative to the buttons on this board (verified on hardware: at 90° the Right
// button must drive Up, and Up must drive Left). MIDDLE is orientation-independent.
uint8_t E32KeyMap::rotateId(uint8_t id) const {
    if (rotation_ == 0) return id;
    static const uint8_t ring[4] = {BTN_UP, BTN_RIGHT, BTN_DOWN, BTN_LEFT};
    for (int i = 0; i < 4; i++)
        if (ring[i] == id) return ring[(i + 4 - rotation_) & 3];
    return id;
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
        case Action::Prev:       return "Up";       // side top    (navigate prev)
        case Action::Next:       return "Dn";       // side bottom (navigate next)
        case Action::Activate:   return "OK";       // center (SW2), single tap
        case Action::Back:       return "2x OK";    // center (SW2), double tap
        case Action::Menu:       return "Hold OK";  // center (SW2), long-hold
        case Action::AdjustUp:   return "Right";    // below right (value up)
        case Action::AdjustDown: return "Left";     // below left  (value down)
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
        case Code::Escape:
        case Code::Menu:   return true;
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
        case Action::AdjustDown:
        case Action::Menu:       return true;
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
            if (g == Gesture::Hold)   return Code::Menu;     // context menu
            if (g == Gesture::Double) return Code::Escape;   // back
            return Code::Enter;                              // tap = OK
        default:         return Code::None;
    }
}

// static
Action E32KeyMap::idToAction(uint8_t id, Gesture g) {
    switch (id) {
        // Side Up/Down = PRIMARY navigation. Below-screen Left/Right = SECONDARY
        // adjust (change a focused value/stepper/dropdown; falls back to nav in
        // the component runtime when nothing is adjustable). This mirrors the
        // simulator's defaultAction() so input behaves identically on both boards.
        case BTN_UP:     return Action::Prev;        // navigate up / previous
        case BTN_DOWN:   return Action::Next;        // navigate down / next
        case BTN_LEFT:   return Action::AdjustDown;  // value down / left
        case BTN_RIGHT:  return Action::AdjustUp;    // value up / right
        case BTN_MIDDLE:
            // Board gesture profile: tap = OK, double = Back, long-hold = Menu.
            if (g == Gesture::Hold)   return Action::Menu;
            if (g == Gesture::Double) return Action::Back;
            if (g == Gesture::Short)  return Action::Activate;
            return Action::None;
        default:
            return Action::None;
    }
}

} // namespace nema::skyrizze32
