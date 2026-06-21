#include "nema/services/display_power_manager.h"
#include "nema/ui/screen.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/hal/display.h"
#include "nema/clock.h"

namespace nema {

void DisplayPowerManager::init(ViewDispatcher& vd, IDisplayDriver* display,
                                IClock& clock, IScreen& lockScreen,
                                uint64_t sleepMs, uint64_t lockMs) {
    vd_          = &vd;
    display_     = display;
    clock_       = &clock;
    lockScreen_  = &lockScreen;
    sleepTimeoutMs_ = sleepMs;
    lockTimeoutMs_  = lockMs;
    lastActivityMs_ = clock.millis();
}

bool DisplayPowerManager::deliverKey(Key k, uint64_t nowMs) {
    switch (state_) {
    case State::Active:
        lastActivityMs_ = nowMs;
        return false;

    case State::Sleep:
        wake(nowMs);
        return true;

    case State::Locked:
        if (!lockScreenShown_) {
            // First press while locked: wake display and reveal lock screen.
            // This key is consumed as the "wake" tap — not forwarded as an unlock gesture.
            if (display_) display_->wake();
            if (vd_ && lockScreen_) { vd_->push(*lockScreen_); vd_->requestRedraw(); }
            lockScreenShown_ = true;
            return true;
        }
        if (vd_) vd_->handleKey(k);
        return true;
    }
    return false;
}

void DisplayPowerManager::tick(uint64_t nowMs) {
    switch (state_) {
    case State::Active:
        if (sleepTimeoutMs_ != kNever && nowMs - lastActivityMs_ >= sleepTimeoutMs_)
            enterSleep(nowMs);
        break;
    case State::Sleep:
        if (lockTimeoutMs_ != kNever && nowMs - sleepEnterMs_ >= lockTimeoutMs_)
            enterLocked();
        break;
    case State::Locked:
        break;
    }
}

void DisplayPowerManager::unlock() {
    if (vd_) vd_->pop();
    state_           = State::Active;
    lockScreenShown_ = false;
    lastActivityMs_  = clock_ ? clock_->millis() : 0;
    if (vd_) vd_->requestRedraw();
}

bool DisplayPowerManager::isActive()   const { return state_ == State::Active; }
bool DisplayPowerManager::isSleeping() const { return state_ == State::Sleep;  }
bool DisplayPowerManager::isLocked()   const { return state_ == State::Locked; }

bool DisplayPowerManager::takeEnteredSleep() {
    bool v = enteredSleepFlag_;
    enteredSleepFlag_ = false;
    return v;
}

void DisplayPowerManager::enterSleep(uint64_t nowMs) {
    state_            = State::Sleep;
    sleepEnterMs_     = nowMs;
    enteredSleepFlag_ = true;
    if (display_) display_->sleep();
}

void DisplayPowerManager::enterLocked() {
    state_ = State::Locked;
    lockScreenShown_ = false;
    // Display stays off (already sleeping from enterSleep()).
    // LockScreen is shown lazily on the first key press — see deliverKey().
}

void DisplayPowerManager::wake(uint64_t nowMs) {
    state_          = State::Active;
    lastActivityMs_ = nowMs;
    if (display_) display_->wake();
    if (vd_) vd_->requestRedraw();
}

} // namespace nema
