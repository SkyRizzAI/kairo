#pragma once
#include "kairo/service.h"
#include "kairo/ui/key.h"
#include "kairo/nema/thread.h"
#include <cstdint>

namespace kairo {

class Runtime;
class InputService;

// TCA9534 6-button driver.
//
// Polls the I²C expander in its OWN nema::Thread (not the main loop). Because
// this thread is never blocked by e-ink refresh or app work, a button edge
// is captured and posted to InputService the instant it happens — no lost or
// "jumped" presses. The main task drains InputService at its own pace.
class TCA9534Buttons : public IService {
public:
    void init(Runtime& rt);

    const char* name() const override { return "TCA9534Buttons"; }
    void start() override;   // configure expander + spawn poll thread
    void stop()  override;   // stop poll thread
    void tick(uint64_t) override {}   // nothing in main loop anymore

private:
    static void pollThread(void* arg);  // nema::Thread entry
    void        pollOnce();             // one I²C read + edge dispatch
    uint8_t     readRaw();
    static Key  bitToKey(uint8_t bit);

    Runtime*      rt_    = nullptr;
    InputService* input_ = nullptr;
    nema::Thread  thread_;
    uint8_t       last_  = 0;
};

} // namespace kairo
