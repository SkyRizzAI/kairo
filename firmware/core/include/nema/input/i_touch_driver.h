#pragma once
#include "nema/input/pointer.h"
#include <cstdint>

namespace nema {

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

    // Match the active display rotation so touch coords stay aligned with the
    // rotated UI (Plan 92 Fase A). Default no-op (boards without touch rotation).
    virtual void setRotation(uint8_t /*r*/) {}

protected:
    // Subclasses call this when a touch transition is detected. Coordinates
    // MUST already be logical (post-transform). Posts into InputService.
    void emitPointer(input::PointerPhase phase, uint16_t x, uint16_t y);

    InputService* input_ = nullptr;
};

} // namespace nema
