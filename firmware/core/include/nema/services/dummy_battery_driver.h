#pragma once
#include "nema/hal/battery.h"
#include "nema/service.h"

namespace nema {

class Runtime;
class EventBus;

// DummyBatteryDriver — returns hardcoded 85% discharging. Fallback for boards
// without battery ADC hardware. Publishes `BatteryChanged` event periodically
// so the status bar icon renders rather than showing blank.
class DummyBatteryDriver : public IBatteryDriver, public IService {
public:
    void onRegister(Runtime& rt) override;

    const char* name() const override { return "DummyBatteryDriver"; }
    DriverKind  kind() const override { return DriverKind::Battery; }
    void start() override;
    void stop() override {};
    void tick(uint64_t nowMs) override;

    int  level() const override { return level_; }
    bool isCharging() const override { return charging_; }

private:
    Runtime*    rt_ = nullptr;
    EventBus*   bus_ = nullptr;
    int         level_ = 85;
    bool        charging_ = false;
    uint64_t    lastTickMs_ = 0;
};

} // namespace nema
