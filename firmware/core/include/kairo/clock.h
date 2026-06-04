#pragma once
#include <cstdint>

namespace kairo {

// Abstraction over time — core never calls std::chrono directly.
// Platform provides the concrete implementation.
struct IClock {
    virtual ~IClock() = default;
    virtual uint64_t millis() = 0;    // monotonic ms since process start
    virtual uint64_t epochMs() = 0;   // wall-clock ms (UTC) for log timestamps
};

} // namespace kairo
