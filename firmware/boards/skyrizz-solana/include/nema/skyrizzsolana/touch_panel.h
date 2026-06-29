#pragma once
#include "nema/service.h"
#include "nema/input/i_touch_driver.h"
#include <cstdint>

namespace nema {
class Runtime;
}

namespace nema::skyrizzsolana {

// TouchPanel — auto-detecting touch driver for SkyRizz Solana.
//
// The board can be fitted with EITHER a capacitive FT6336U (@0x38) or a resistive
// TSC2007 (@0x4A); both controllers appear on the schematic. start() pulses the
// shared touch-reset (GPIO6, direct) then probes each address and drives whichever
// ACKs. Emits PointerEvents in LOGICAL canvas coordinates (Plan 29) — rotation,
// scaling and (for the resistive panel) calibration are applied internally so the
// component layer never sees raw hardware values.
class TouchPanel : public IService, public ITouchDriver {
public:
    enum class Controller : uint8_t { None, Ft6336, Tsc2007 };

    void init(Runtime& rt);

    const char* name() const override { return "TouchPanel"; }
    void start() override;            // reset pulse + probe both controllers
    void stop()  override {}
    void tick(uint64_t nowMs) override;

    void setRotation(uint8_t r) override { rotation_ = (uint8_t)(r & 3); }

    Controller controller() const { return ctrl_; }

private:
    bool readFt6336 (uint16_t& x, uint16_t& y, bool& down);
    bool readTsc2007(uint16_t& x, uint16_t& y, bool& down);
    void toLogical(uint16_t rawX, uint16_t rawY, uint16_t& lx, uint16_t& ly);

    Runtime*   rt_       = nullptr;
    Controller ctrl_     = Controller::None;
    uint64_t   lastPoll_ = 0;
    bool       wasDown_  = false;
    uint16_t   lastX_    = 0;
    uint16_t   lastY_    = 0;
    uint8_t    rotation_ = 0;
};

} // namespace nema::skyrizzsolana
