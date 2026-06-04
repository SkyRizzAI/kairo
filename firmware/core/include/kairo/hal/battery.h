#pragma once
#include "kairo/hal/driver.h"

namespace kairo {

struct IBatteryDriver : IDriver {
    virtual int  level()      const = 0;  // 0–100
    virtual bool isCharging() const = 0;
};

} // namespace kairo
