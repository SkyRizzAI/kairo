#pragma once
#include "nema/clock.h"
#include <cstdint>

namespace nema {

class Esp32Clock : public IClock {
public:
    uint64_t millis() override;   // esp_timer_get_time() / 1000
    uint64_t epochMs() override;  // gettimeofday
};

} // namespace nema
