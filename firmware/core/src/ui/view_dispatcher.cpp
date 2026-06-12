#include "nema/ui/view_dispatcher.h"
#include "nema/ui/screen.h"
#include "nema/input/input_code.h"
#include "nema/input/input_action.h"

namespace nema {

void ViewDispatcher::push(IScreen& screen) {
    stack_.push_back(&screen);
    screen.enter();
    requestRedraw();
}

void ViewDispatcher::pop() {
    if (stack_.size() <= 1) return;  // don't pop the last screen
    stack_.pop_back();
    if (!stack_.empty()) {
        stack_.back()->enter();
        requestRedraw();
    }
}

void ViewDispatcher::popToRoot() {
    if (stack_.size() <= 1) return;
    stack_.resize(1);            // drop everything above the base (Home)
    stack_.back()->enter();
    requestRedraw();
}

IScreen* ViewDispatcher::active() const {
    return stack_.empty() ? nullptr : stack_.back();
}

IScreen* ViewDispatcher::previous() const {
    return stack_.size() >= 2 ? stack_[stack_.size() - 2] : nullptr;
}

bool ViewDispatcher::empty() const { return stack_.empty(); }

void ViewDispatcher::requestRedraw() { redrawPending_.store(true, std::memory_order_release); }

bool ViewDispatcher::takeRedraw() {
    return redrawPending_.exchange(false, std::memory_order_acquire);
}

void ViewDispatcher::handleAction(input::Action a) {
    if (auto* s = active()) s->onAction(a);
}

void ViewDispatcher::handleCode(input::Code c) {
    if (auto* s = active()) s->onCode(c);
}

void ViewDispatcher::handlePointer(const input::PointerEvent& e) {
    if (auto* s = active()) s->onPointer(e);
}

void ViewDispatcher::handleKey(Key key) {
    // Legacy path — converts to Action and goes through the primary route.
    handleAction(input::defaultAction(input::codeFromKey(key)));
}

void ViewDispatcher::tick(uint64_t nowMs) {
    if (auto* s = active()) s->tick(nowMs);
}

} // namespace nema
