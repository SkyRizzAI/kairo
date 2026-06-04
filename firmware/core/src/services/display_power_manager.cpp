#include "kairo/services/display_power_manager.h"
#include "kairo/screens/lock_screen.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/hal/display.h"
#include "kairo/clock.h"

namespace kairo {

void DisplayPowerManager::init(ViewDispatcher& vd, IDisplayDriver* display,
                                IClock& clock, LockScreen& lockScreen,
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
        if (vd_) vd_->handleKey(k);  // LockScreen at top of stack handles it
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
    state_          = State::Active;
    lastActivityMs_ = clock_ ? clock_->millis() : 0;
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
    if (display_) display_->wake();  // display is active again (LockScreen renders)
    if (vd_ && lockScreen_) {
        vd_->push(*lockScreen_);
        vd_->requestRedraw();
    }
}

void DisplayPowerManager::wake(uint64_t nowMs) {
    state_          = State::Active;
    lastActivityMs_ = nowMs;
    if (display_) display_->wake();
    if (vd_) vd_->requestRedraw();
}

} // namespace kairo
