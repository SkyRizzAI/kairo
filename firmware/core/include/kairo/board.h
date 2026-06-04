#pragma once

namespace kairo {

class Runtime;

struct IBoard {
    virtual ~IBoard() = default;
    virtual const char* name() const = 0;
    // Declare hardware and capabilities into the runtime registries.
    // Called during Runtime::registerServices(), after platform drivers.
    virtual void describeHardware(Runtime& rt) = 0;
};

} // namespace kairo
