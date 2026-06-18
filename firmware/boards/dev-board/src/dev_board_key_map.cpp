#include "nema/devboard/dev_board_key_map.h"

namespace nema {

using namespace input;

void DevBoardKeyMap::feedEdge(uint8_t id, bool pressed, uint64_t nowMs) {
    // Cancel: decide on RELEASE so a long hold can mean Pause (Plan 22).
    if (id == BTN_CANCEL) {
        if (pressed) { cancelPressMs_ = nowMs; return; }
        uint64_t held = nowMs - cancelPressMs_;
        if (held >= HOLD_MS) emitEvent(Code::None,   Action::Pause, Gesture::Hold,  nowMs);
        else                 emitEvent(Code::Escape, Action::Back,  Gesture::Short, nowMs);
        return;
    }
    if (!pressed) return;  // other buttons: act on press (rising edge)
    Code   c = idToCode(id);
    Action a = idToAction(id);
    if (c != Code::None) emitEvent(c, a, Gesture::Short, nowMs);
}

const char* DevBoardKeyMap::buttonLabel(uint8_t id) const {
    switch (id) {
        case BTN_LEFT:   return "Left";
        case BTN_DOWN:   return "Down";
        case BTN_UP:     return "Up";
        case BTN_RIGHT:  return "Right";
        case BTN_SELECT: return "Select";
        case BTN_CANCEL: return "Cancel";
        default:         return "?";
    }
}

const char* DevBoardKeyMap::hintFor(Action a) const {
    switch (a) {
        case Action::Prev:       return "Left";
        case Action::Next:       return "Right";
        case Action::Activate:   return "Select";
        case Action::Back:       return "Cancel";
        case Action::AdjustUp:   return "Up";
        case Action::AdjustDown: return "Down";
        default:                 return "";
    }
}

bool DevBoardKeyMap::hasCode(Code c) const {
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

Code DevBoardKeyMap::idToCode(uint8_t id) {
    switch (id) {
        case BTN_LEFT:   return Code::Left;
        case BTN_DOWN:   return Code::Down;
        case BTN_UP:     return Code::Up;
        case BTN_RIGHT:  return Code::Right;
        case BTN_SELECT: return Code::Enter;
        case BTN_CANCEL: return Code::Escape;
        default:         return Code::None;
    }
}

Action DevBoardKeyMap::idToAction(uint8_t id) {
    switch (id) {
        case BTN_LEFT:   return Action::Prev;
        case BTN_DOWN:   return Action::AdjustDown;
        case BTN_UP:     return Action::AdjustUp;
        case BTN_RIGHT:  return Action::Next;
        case BTN_SELECT: return Action::Activate;
        case BTN_CANCEL: return Action::Back;
        default:         return Action::None;
    }
}

} // namespace nema
