#pragma once
#include "kairo/input/pointer.h"
#include <cstdint>

namespace kairo {

class InputService;

// ITouchDriver — per-board touch HAL, the pointer-side parallel of IKeyMap.
//
// A board with a touch controller implements this (e.g. FT6336U on SkyRizz E32)
// and registers it + declares the "input.touch" capability. The driver absorbs
// ALL controller-specific concerns and emits PointerEvents already in LOGICAL
// canvas coordinates (orientation + scale + calibration applied internally), so
// the component layer never sees raw hardware values.
class ITouchDriver {
public:
    virtual ~ITouchDriver() = default;

    // Wire the funnel (mirrors IKeyMap::attachInput). Not owning.
    void attachInput(InputService* svc) { input_ = svc; }

protected:
    // Subclasses call this when a touch transition is detected. Coordinates
    // MUST already be logical (post-transform). Posts into InputService.
    void emitPointer(input::PointerPhase phase, uint16_t x, uint16_t y);

    InputService* input_ = nullptr;
};

} // namespace kairo
