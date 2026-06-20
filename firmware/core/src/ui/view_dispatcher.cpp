#include "nema/ui/view_dispatcher.h"
#include "nema/ui/screen.h"
#include "nema/input/input_code.h"
#include "nema/input/input_action.h"

namespace nema {

// ── Plan 70: Android-style navigation API ─────────────────────────────────────

void ViewDispatcher::navigate(IScreen& screen) {
    if (auto* cur = active()) cur->onPause();
    stack_.push_back(&screen);
    screen.onResume();
    requestRedraw();
}


void ViewDispatcher::navigate(IScreen& screen, Bundle args) {
    args_ = std::move(args);
    navigate(screen);
}

void ViewDispatcher::replace(IScreen& screen) {
    if (auto* cur = active()) {
        cur->onPause();
        cur->onStop();
        stack_.pop_back();
    }
    stack_.push_back(&screen);
    screen.onResume();
    requestRedraw();
}

bool ViewDispatcher::goBack() {
    if (stack_.size() <= 1) return false;
    if (auto* cur = active()) {
        cur->onPause();
        cur->onStop();
    }
    stack_.pop_back();
    if (!stack_.empty()) {
        stack_.back()->onResume();
        requestRedraw();
    }
    return true;
}

bool ViewDispatcher::canGoBack() const { return stack_.size() > 1; }

void ViewDispatcher::clearBackStack() {
    if (stack_.size() <= 1) return;
    IScreen* top = stack_.back();
    stack_.clear();
    stack_.push_back(top);
}

void ViewDispatcher::popTo(IScreen& target) {
    while (stack_.size() > 1 && stack_.back() != &target) {
        stack_.back()->onStop();
        stack_.pop_back();
    }
    if (stack_.back() == &target) {
        stack_.back()->onResume();
        requestRedraw();
    }
}

// ── Legacy API (deprecated, Plan 70: forwards to new API) ──────────────────────

void ViewDispatcher::push(IScreen& screen) {
    navigate(screen);
}

void ViewDispatcher::pop() {
    goBack();
}

void ViewDispatcher::popToRoot() {
    if (stack_.size() <= 1) return;
    while (stack_.size() > 1) {
        stack_.back()->onStop();
        stack_.pop_back();
    }
    stack_.back()->onResume();
    requestRedraw();
}

IScreen* ViewDispatcher::active() const {
    return stack_.empty() ? nullptr : stack_.back();
}

IScreen* ViewDispatcher::previous() const {
    return stack_.size() >= 2 ? stack_[stack_.size() - 2] : nullptr;
}

bool ViewDispatcher::empty() const { return stack_.empty(); }

void ViewDispatcher::requestRedraw() {
    dirtyAll_ = true;
    hasDirty_ = false;
    redrawPending_.store(true, std::memory_order_release);
}

void ViewDispatcher::requestRedraw(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (dirtyAll_) return;   // already full redraw
    if (!hasDirty_) {
        dirtyX_ = x; dirtyY_ = y; dirtyW_ = w; dirtyH_ = h;
        hasDirty_ = true;
    } else {
        // Union: expand the bounding box to include the new rect
        uint16_t x2 = (uint16_t)(x + w), y2 = (uint16_t)(y + h);
        uint16_t dx2 = (uint16_t)(dirtyX_ + dirtyW_), dy2 = (uint16_t)(dirtyY_ + dirtyH_);
        if (x < dirtyX_) dirtyX_ = x;
        if (y < dirtyY_) dirtyY_ = y;
        if (x2 > dx2) dirtyW_ = (uint16_t)(x2 - dirtyX_);
        if (y2 > dy2) dirtyH_ = (uint16_t)(y2 - dirtyY_);
    }
    redrawPending_.store(true, std::memory_order_release);
}

bool ViewDispatcher::takeRedraw() {
    bool was = redrawPending_.exchange(false, std::memory_order_acquire);
    if (was) {
        // Consume: read dirty state for this frame, then reset for next
        hasDirty_  = false;
    }
    return was;
}

bool ViewDispatcher::getDirtyBounds(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h) const {
    if (dirtyAll_ || !hasDirty_) return false;   // full redraw
    x = dirtyX_; y = dirtyY_; w = dirtyW_; h = dirtyH_;
    return true;
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
