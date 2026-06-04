#pragma once
#include "kairo/board.h"

namespace kairo {

class SimulatorBoard : public IBoard {
public:
    const char* name() const override { return "simulator"; }
    void describeHardware(Runtime& rt) override;
};

} // namespace kairo
