#pragma once
#include <cstdint>

namespace kairo {

struct IService {
    virtual ~IService() = default;
    virtual const char* name() const = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    // Optional tick called each loop iteration. nowMs is monotonic ms from IClock.
    virtual void tick(uint64_t /*nowMs*/) {}
};

} // namespace kairo
