#include "nema/esp32/esp32_clock.h"
#include <esp_timer.h>
#include <sys/time.h>

namespace nema {

uint64_t Esp32Clock::millis() {
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

uint64_t Esp32Clock::epochMs() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
}

void Esp32Clock::setEpochMs(uint64_t epochMs) {
    struct timeval tv;
    tv.tv_sec = (time_t)(epochMs / 1000ULL);
    tv.tv_usec = (suseconds_t)((epochMs % 1000ULL) * 1000ULL);
    settimeofday(&tv, nullptr);
}

} // namespace nema
