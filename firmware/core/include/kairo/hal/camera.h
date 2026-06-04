#pragma once
#include <cstdint>

namespace kairo {

// ICamera — camera abstraction. Drivers implement IService + ICamera.
// captureFrame() returns a pointer to an internal RGB565 buffer (big-endian,
// width*height*2 bytes) or nullptr if no frame is ready.
// The pointer is valid until the next captureFrame() call.
struct ICamera {
    virtual ~ICamera() = default;
    virtual const char*    label()        const = 0;  // "Front Camera"
    virtual uint16_t       frameWidth()   const = 0;
    virtual uint16_t       frameHeight()  const = 0;
    virtual bool           isOpen()       const = 0;
    virtual bool           open()               = 0;  // init sensor, start DMA
    virtual void           close()              = 0;
    virtual const uint8_t* captureFrame()       = 0;  // blocks until next frame
};

} // namespace kairo
