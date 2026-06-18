#pragma once
#include "nema/hal/driver.h"
#include <cstdint>
#include <cstddef>

namespace nema {

struct IUsbHid : IDriver {
    DriverKind kind() const override { return DriverKind::Other; }

    virtual void sendKey(uint8_t modifier, uint8_t keycode) = 0;
    virtual void sendString(const char* s, uint32_t delayMs = 0) = 0;
    virtual void delay(uint32_t ms) {}
    virtual void releaseAll() = 0;
    virtual bool isReady() const = 0;
};

} // namespace nema
