#include "kairo/esp32/esp32_clock.h"
#include <esp_timer.h>
#include <sys/time.h>

namespace kairo {

uint64_t Esp32Clock::millis() {
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

uint64_t Esp32Clock::epochMs() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
}

} // namespace kairo
