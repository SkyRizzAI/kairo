#include "nema/sim/simulator_board.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/system/capability_registry.h"

namespace nema {

void SimulatorBoard::describeHardware(Runtime& rt) {
    // Hardware registered by SimulatorPlatform::registerDrivers already.
    // Board can augment or override here. For now just log the summary.
    rt.log().info("SimulatorBoard", "Hardware described");

    // Demo: capability-driven pattern check
    if (rt.capabilities().has("wifi")) {
        rt.log().debug("SimulatorBoard", "wifi capability confirmed");
    }
    if (rt.capabilities().has("nfc")) {
        rt.log().debug("SimulatorBoard", "nfc capability confirmed");
    } else {
        rt.log().debug("SimulatorBoard", "nfc not available on this board");
    }
}

} // namespace nema
