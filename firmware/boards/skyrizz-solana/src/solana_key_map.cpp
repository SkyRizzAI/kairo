#include "nema/skyrizzsolana/solana_key_map.h"
#include "nema/skyrizzsolana/board_config.h"

namespace nema::skyrizzsolana {

using namespace input;

SolanaKeyMap::SolanaKeyMap() {
    engine_.setCallback(&SolanaKeyMap::onGesture, this);
    // OK is two-stage (no repeat): quick tap = Short → Activate, hold past holdMs
    // = Hold → Menu. Back is its own button, so OK doesn't need double-tap.
    engine_.setTwoStage(BTN_OK);
}

void SolanaKeyMap::feedEdge(uint8_t buttonId, bool pressed, uint64_t nowMs) {
    if (buttonId >= 6) return;
    engine_.feedEdge(buttonId, pressed, nowMs);
}

void SolanaKeyMap::tick(uint64_t nowMs) {
    engine_.tick(nowMs);
}

// static
void SolanaKeyMap::onGesture(void* ctx, uint8_t id, Gesture g, uint64_t now) {
    auto* self = static_cast<SolanaKeyMap*>(ctx);
    uint8_t eid = self->rotateId(id);    // follow display rotation (Plan 92 Fase A)
    Code   c = idToCode(eid, g);
    Action a = idToAction(eid, g);
    if (a != Action::None) self->emitEvent(c, a, g, now);
}

// Rotate the 4 directional buttons to match the display orientation so input
// feels natural after the device is physically turned. Ring order Up→Right→Down→
// Left; we step the OPPOSITE way (`-rotation_`) to match the LCD MADCTL set.
// OK/Back are orientation-independent.
uint8_t SolanaKeyMap::rotateId(uint8_t id) const {
    if (rotation_ == 0) return id;
    static const uint8_t ring[4] = {BTN_UP, BTN_RIGHT, BTN_DOWN, BTN_LEFT};
    for (int i = 0; i < 4; i++)
        if (ring[i] == id) return ring[(i + 4 - rotation_) & 3];
    return id;
}

const char* SolanaKeyMap::buttonLabel(uint8_t id) const {
    switch (id) {
        case BTN_LEFT:  return "Left";
        case BTN_DOWN:  return "Down";
        case BTN_UP:    return "Up";
        case BTN_RIGHT: return "Right";
        case BTN_OK:    return "OK";
        case BTN_BACK:  return "Back";
        default:        return "?";
    }
}

const char* SolanaKeyMap::hintFor(Action a) const {
    switch (a) {
        case Action::Prev:       return "Up";
        case Action::Next:       return "Dn";
        case Action::Activate:   return "OK";
        case Action::Back:       return "Back";
        case Action::Menu:       return "Hold OK";
        case Action::AdjustUp:   return "Right";
        case Action::AdjustDown: return "Left";
        default:                 return "";
    }
}

bool SolanaKeyMap::hasCode(Code c) const {
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

bool SolanaKeyMap::canReach(Action a) const {
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
Code SolanaKeyMap::idToCode(uint8_t id, Gesture g) {
    switch (id) {
        case BTN_LEFT:  return Code::Left;
        case BTN_RIGHT: return Code::Right;
        case BTN_UP:    return Code::Up;
        case BTN_DOWN:  return Code::Down;
        case BTN_OK:    return (g == Gesture::Hold) ? Code::Menu : Code::Enter;
        case BTN_BACK:  return Code::Escape;
        default:        return Code::None;
    }
}

// static
Action SolanaKeyMap::idToAction(uint8_t id, Gesture g) {
    switch (id) {
        // D-pad Up/Down = PRIMARY navigation. Left/Right = SECONDARY adjust
        // (change a focused value/stepper/dropdown; falls back to nav in the
        // component runtime when nothing is adjustable). Mirrors the simulator's
        // defaultAction() so input behaves identically on both.
        case BTN_UP:    return Action::Prev;
        case BTN_DOWN:  return Action::Next;
        case BTN_LEFT:  return Action::AdjustDown;
        case BTN_RIGHT: return Action::AdjustUp;
        case BTN_OK:
            // Two-stage: tap = Activate, long-hold = Menu.
            if (g == Gesture::Hold) return Action::Menu;
            if (g == Gesture::Short) return Action::Activate;
            return Action::None;   // ignore the Long stage between Short and Hold
        case BTN_BACK:
            return (g == Gesture::Short) ? Action::Back : Action::None;
        default:
            return Action::None;
    }
}

} // namespace nema::skyrizzsolana
