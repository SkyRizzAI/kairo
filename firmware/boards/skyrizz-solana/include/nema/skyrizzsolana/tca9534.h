#pragma once
#include "nema/service.h"
#include "nema/input/i_key_map.h"
#include <cstdint>

namespace nema {
class Runtime;
}

namespace nema::skyrizzsolana {

// Tca9534 — 8-bit I²C GPIO expander (@0x20).
//
// On SkyRizz Solana the expander carries ONLY the six push buttons (PB1..PB6 on
// P0..P5) — unlike the E32's XL9535, it does NOT drive backlight/resets (those
// are direct ESP32 GPIOs here). The INT# line (GPIO4 PBINT) drops on any button
// change; we set a flag in the ISR and read the input port on tick() (no I²C in
// ISR context), then also poll every 15 ms so a HELD button still produces the
// long-press/repeat the gesture engine needs.
class Tca9534 : public IService {
public:
    void init(Runtime& rt);

    const char* name() const override { return "Tca9534"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t nowMs) override;

    // Set a keymap to receive button events from tick().
    void setKeyMap(input::IKeyMap* km) { keyMap_ = km; }

    // Called from GPIO4 ISR — sets flag, processed in tick().
    static void isrHandler(void* arg);

private:
    uint8_t readInputs();   // raw input port (active-LOW buttons, not inverted)

    Runtime*        rt_       = nullptr;
    input::IKeyMap* keyMap_   = nullptr;
    volatile bool   intFlag_  = false;
    uint64_t        lastPoll_   = 0;   // last input re-read (ms) for hold detection
    uint8_t         lastInputs_ = 0;   // active-HIGH (post-invert), masked to PB bits
};

} // namespace nema::skyrizzsolana
