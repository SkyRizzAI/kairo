#include "kairo/input/gesture.h"
#include <cstring>

namespace kairo::input {

void GestureEngine::feedEdge(uint8_t id, bool pressed, uint64_t now) {
    if (id >= MAX_BUTTONS) return;
    ButtonState& s = states_[id];
    // Double+hold mode (e.g. one button = tap/double/hold → OK/Back/Pause).
    if (doubleHold_[id]) {
        if (pressed) {
            if (s.pendingShort && (now - s.lastTapTime) <= doubleMs) {
                // Second tap inside the window → Double (consume this press).
                s.pendingShort = false;
                s.consumed = true; s.isPressed = true; s.pressTime = now; s.holdFired = false;
                fire(id, Gesture::Double, now);
            } else {
                s.isPressed = true; s.pressTime = now; s.holdFired = false; s.consumed = false;
            }
        } else if (s.isPressed) {
            if (!s.holdFired && !s.consumed) {
                // Quick tap completed → defer Short until the double window lapses.
                s.pendingShort = true; s.lastTapTime = now;
            }
            s.isPressed = false; s.holdFired = false; s.consumed = false;
        }
        return;
    }

    if (pressed) {
        s.pressTime  = now;
        s.isPressed  = true;
        s.longFired  = false;
        s.holdFired  = false;
        s.lastRepeat = 0;
    } else {
        if (s.isPressed) {
            if (twoStage_[id]) {
                // Two-stage: nothing fired at threshold (no repeat). Decide on
                // release: Hold already fired → nothing; else Long vs Short by
                // duration. This lets tap/hold/long-hold coexist on one button.
                if (!s.holdFired) {
                    uint64_t dur = now - s.pressTime;
                    fire(id, dur >= longMs ? Gesture::Long : Gesture::Short, now);
                }
            } else if (!s.longFired) {
                // Released before long threshold → Short
                fire(id, Gesture::Short, now);
            }
            // If longFired (non-two-stage), Long was emitted on threshold; no Short.
        }
        s.isPressed = false;
        s.longFired = false;
        s.holdFired = false;
    }
}

void GestureEngine::tick(uint64_t now) {
    for (uint8_t id = 0; id < MAX_BUTTONS; id++) {
        ButtonState& s = states_[id];

        // Double+hold: fire Hold while held; confirm a single Short once the
        // double window passes with no second tap. (Runs even when released.)
        if (doubleHold_[id]) {
            if (s.isPressed && !s.holdFired && (now - s.pressTime) >= holdMs) {
                s.holdFired = true; s.pendingShort = false;
                fire(id, Gesture::Hold, now);
            }
            if (s.pendingShort && (now - s.lastTapTime) > doubleMs) {
                s.pendingShort = false;
                fire(id, Gesture::Short, now);
            }
            continue;
        }

        if (!s.isPressed) continue;

        uint64_t held = now - s.pressTime;

        if (twoStage_[id]) {
            // Only the long-hold stage fires at threshold (while still held);
            // Short/Long are decided on release. No repeat.
            if (!s.holdFired && held >= holdMs) {
                s.holdFired = true;
                fire(id, Gesture::Hold, now);
            }
            continue;
        }

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
