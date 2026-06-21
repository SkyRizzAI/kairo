#pragma once
#include "nema/ui/key.h"
#include <cstdint>

namespace nema {

class ViewDispatcher;
struct IDisplayDriver;
struct IClock;
struct IScreen;

class DisplayPowerManager {
public:
    void init(ViewDispatcher& vd, IDisplayDriver* display, IClock& clock,
              IScreen& lockScreen,
              uint64_t sleepMs = 15000, uint64_t lockMs = 15000);

    // Use as sleepMs / lockMs value to disable that timeout entirely.
    static constexpr uint64_t kNever = UINT64_MAX;

    // Returns true = key consumed (caller must NOT forward to vd.handleKey).
    bool deliverKey(Key k, uint64_t nowMs);

    void tick(uint64_t nowMs);

    // Called by LockScreen on Select×2 — must run on GuiService thread.
    void unlock();

    bool isActive()        const;
    bool isSleeping()      const;
    bool isLocked()        const;
    bool takeEnteredSleep();  // one-shot: true the first frame after sleep entry

    uint64_t sleepMs() const { return sleepTimeoutMs_; }
    uint64_t lockMs()  const { return lockTimeoutMs_;  }
    void setSleepMs(uint64_t ms) { sleepTimeoutMs_ = ms; }
    void setLockMs (uint64_t ms) { lockTimeoutMs_  = ms; }

private:
    enum class State { Active, Sleep, Locked };

    void enterSleep(uint64_t nowMs);
    void enterLocked();
    void wake(uint64_t nowMs);

    State          state_            = State::Active;
    uint64_t       lastActivityMs_   = 0;
    uint64_t       sleepEnterMs_     = 0;
    uint64_t       sleepTimeoutMs_   = 15000;
    uint64_t       lockTimeoutMs_    = 15000;
    bool           enteredSleepFlag_ = false;

    ViewDispatcher*  vd_         = nullptr;
    IDisplayDriver*  display_    = nullptr;
    IClock*          clock_      = nullptr;
    IScreen*         lockScreen_ = nullptr;
};

} // namespace nema
