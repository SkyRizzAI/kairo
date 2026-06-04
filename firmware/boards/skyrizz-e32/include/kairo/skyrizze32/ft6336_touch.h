#pragma once
#include "kairo/service.h"
#include "kairo/input/i_touch_driver.h"
#include <cstdint>

namespace kairo {
class Runtime;
}

namespace kairo::skyrizze32 {

class Xl9535;

// Ft6336Touch — FT6336U capacitive touch driver for SkyRizz E32.
//
// I2C 0x38, PENIRQ GPIO2, reset via XL9535 P01. Polls TD_STATUS in tick(),
// detects touch-down / touch-up transitions, and emits PointerEvents in LOGICAL
// canvas coordinates (Plan 29). Move events are not emitted in v1 (scroll/drag
// comes in Plan 31) — only Down + Up, which is enough for tap → onPress.
class Ft6336Touch : public IService, public ITouchDriver {
public:
    void init(Runtime& rt, Xl9535& expander);

    const char* name() const override { return "Ft6336Touch"; }
    void start() override;            // reset pulse + I2C presence probe
    void stop()  override {}
    void tick(uint64_t nowMs) override;   // poll @ ~15 ms

private:
    bool readPoint(uint16_t& x, uint16_t& y, uint8_t& points);
    void toLogical(uint16_t rawX, uint16_t rawY, uint16_t& lx, uint16_t& ly);

    Runtime* rt_       = nullptr;
    Xl9535*  expander_ = nullptr;
    uint64_t lastPoll_ = 0;
    bool     wasDown_  = false;
    uint16_t lastX_    = 0;
    uint16_t lastY_    = 0;
};

} // namespace kairo::skyrizze32
