#include "nema/system/capabilities.h"
#include "nema/sim/simulator_board.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/system/capability_registry.h"

namespace nema {

void SimulatorBoard::describeHardware(Runtime& rt) {
    // Hardware registered by SimulatorPlatform::registerDrivers already.
    // Board can augment or override here. For now just log the summary.
    rt.log().info("SimulatorBoard", "Hardware described");

    // Virtual RGB LEDs so rt.led() + the LEDs settings screen work in the sim.
    rt.led().addLed(&rgb_, "rgb0", "Virtual RGB x2");
    rt.capabilities().add(caps::Led);
    rt.capabilities().add(caps::LedRgb);

    // Demo: capability-driven pattern check
    if (rt.capabilities().has(caps::NetWifi)) {
        rt.log().debug("SimulatorBoard", "wifi capability confirmed");
    }
    if (rt.capabilities().has("nfc")) {
        rt.log().debug("SimulatorBoard", "nfc capability confirmed");
    } else {
        rt.log().debug("SimulatorBoard", "nfc not available on this board");
    }
}

} // namespace nema
