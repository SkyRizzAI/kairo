#pragma once
#include "nema/system/board_profile.h"

namespace nema {

class Runtime;

// Optional microSD-over-SPI wiring. Boards with an SD socket override sdSpi()
// and return true with their pins; the ESP32 platform then mounts the card at
// "/sd". Plain ints — no platform/IDF dependency, safe in core.
struct SdSpiConfig {
    int sclk   = -1;
    int miso   = -1;
    int mosi   = -1;
    int cs     = -1;
    int hostId = 2;   // ESP32-S3: 2 == SPI3_HOST
};

struct IBoard {
    virtual ~IBoard() = default;
    virtual const char* name() const = 0;
    // Declare hardware and capabilities into the runtime registries.
    // Called during Runtime::registerServices(), after platform drivers.
    virtual void describeHardware(Runtime& rt) = 0;
    // Return the physical layout profile of this board.
    // Describes component positions for visualization (device + Forge).
    virtual const BoardProfile& profile() const = 0;
    // microSD-over-SPI pins. Default: no SD socket. Boards with one override.
    virtual bool sdSpi(SdSpiConfig& /*out*/) const { return false; }
};

} // namespace nema
