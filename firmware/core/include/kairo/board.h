#pragma once
#include "kairo/system/board_profile.h"

namespace kairo {

class Runtime;

struct IBoard {
    virtual ~IBoard() = default;
    virtual const char* name() const = 0;
    // Declare hardware and capabilities into the runtime registries.
    // Called during Runtime::registerServices(), after platform drivers.
    virtual void describeHardware(Runtime& rt) = 0;
    // Return the physical layout profile of this board.
    // Describes component positions for visualization (device + Forge).
    virtual const BoardProfile& profile() const = 0;
};

} // namespace kairo
