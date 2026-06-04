#pragma once
#include "kairo/hal/battery.h"
#include "kairo/service.h"
#include <cstdint>

namespace kairo {

class Logger;
class EventBus;

// Simulated battery: starts at 100%, drains 1% every ~5 seconds.
class SimBatteryDriver : public IBatteryDriver, public IService {
public:
    void init(Logger& log, EventBus& events);

    // IDriver
    const char* name() const override { return "SimBatteryDriver"; }
    DriverKind  kind() const override { return DriverKind::Battery; }

    // IBatteryDriver
    int  level()      const override { return level_; }
    bool isCharging() const override { return false; }

    // IService
    void start() override;
    void stop()  override;
    void tick(uint64_t nowMs) override;

private:
    Logger*   log_    = nullptr;
    EventBus* events_ = nullptr;
    int       level_  = 100;
    uint64_t  lastDrainMs_ = 0;
};

} // namespace kairo
