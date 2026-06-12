#pragma once
#include "nema/hal/driver.h"

namespace nema {

struct IBatteryDriver : IDriver {
    virtual int  level()      const = 0;  // 0–100
    virtual bool isCharging() const = 0;
};

} // namespace nema
