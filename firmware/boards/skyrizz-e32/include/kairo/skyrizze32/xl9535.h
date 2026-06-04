#pragma once
#include "kairo/service.h"
#include "kairo/input/i_key_map.h"
#include <cstdint>

namespace kairo {
class Runtime;
}

namespace kairo::skyrizze32 {

// XL9535 — 16-bit I²C GPIO expander.
//
// Manages all expander I/O: LCD backlight, indicator LED, reset outputs,
// and button inputs (SW1/PB1/SW2 and more). The INT# line (GPIO43) signals
// any port change; we poll on tick() after the ISR sets intFlag_ (avoids
// doing I²C inside the ISR context).
class Xl9535 : public IService {
public:
    void init(Runtime& rt);

    const char* name() const override { return "Xl9535"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t nowMs) override;

    // Output control
    void setBacklight(bool on);       // P00
    void setIndicatorLed(bool on);    // P17
    void setTouchReset(bool asserted); // P01 — assert=LOW
    void setCamReset  (bool asserted); // P02
    void setSeReset   (bool asserted); // P03

    // Read all 16 input bits (active-HIGH after inversion).
    // [bits 15:8] = Port 1 (P17-P10), [bits 7:0] = Port 0 (P07-P00)
    uint16_t readInputs();

    // Set a keymap to receive button events from tick().
    void setKeyMap(input::IKeyMap* km, uint64_t nowMs = 0);

    // Called from GPIO43 ISR — sets flag, processed in tick().
    static void isrHandler(void* arg);

private:
    void     writeOutput(uint8_t port, uint8_t val);
    uint16_t readRaw();      // raw 16-bit (not inverted)

    Runtime*        rt_       = nullptr;
    input::IKeyMap* keyMap_   = nullptr;
    volatile bool   intFlag_  = false;
    uint64_t        lastPoll_   = 0;   // last input re-read (ms) for hold detection
    uint16_t        lastInputs_ = 0;
    uint8_t         out0_     = 0;   // shadow output register port 0
    uint8_t         out1_     = 0;   // shadow output register port 1
};

} // namespace kairo::skyrizze32
