#include "kairo/input/gesture.h"
#include <cstring>

namespace kairo::input {

void GestureEngine::feedEdge(uint8_t id, bool pressed, uint64_t now) {
    if (id >= MAX_BUTTONS) return;
    ButtonState& s = states_[id];
    if (pressed) {
        s.pressTime  = now;
        s.isPressed  = true;
        s.longFired  = false;
        s.lastRepeat = 0;
    } else {
        if (s.isPressed) {
            if (!s.longFired) {
                // Released before long threshold → Short
                fire(id, Gesture::Short, now);
            }
            // If longFired, the Long was already emitted on threshold; no Short.
        }
        s.isPressed = false;
        s.longFired = false;
    }
}

void GestureEngine::tick(uint64_t now) {
    for (uint8_t id = 0; id < MAX_BUTTONS; id++) {
        ButtonState& s = states_[id];
        if (!s.isPressed) continue;

        uint64_t held = now - s.pressTime;
        if (!s.longFired && held >= longMs) {
            s.longFired  = true;
            s.lastRepeat = now;
            fire(id, Gesture::Long, now);
        } else if (s.longFired) {
            if (s.lastRepeat == 0) s.lastRepeat = now;
            if (now - s.lastRepeat >= repeatMs) {
                s.lastRepeat = now;
                fire(id, Gesture::Repeat, now);
            }
        }
    }
}

void GestureEngine::reset() {
    std::memset(states_, 0, sizeof(states_));
}

void GestureEngine::fire(uint8_t id, Gesture g, uint64_t now) {
    if (cb_) cb_(ctx_, id, g, now);
}

} // namespace kairo::input
