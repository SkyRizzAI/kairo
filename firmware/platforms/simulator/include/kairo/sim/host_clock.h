#pragma once
#include "kairo/clock.h"
#include <chrono>

namespace kairo {

class HostClock : public IClock {
    using steady = std::chrono::steady_clock;
    using system = std::chrono::system_clock;
    using ms     = std::chrono::milliseconds;

    uint64_t startMs_;
public:
    HostClock()
        : startMs_(static_cast<uint64_t>(
              std::chrono::duration_cast<ms>(steady::now().time_since_epoch()).count())) {}

    uint64_t millis() override {
        auto now = static_cast<uint64_t>(
            std::chrono::duration_cast<ms>(steady::now().time_since_epoch()).count());
        return now - startMs_;
    }
    uint64_t epochMs() override {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<ms>(system::now().time_since_epoch()).count());
    }
};

} // namespace kairo
