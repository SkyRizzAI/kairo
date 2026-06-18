#include "nema/services/dummy_battery_driver.h"
#include "nema/runtime.h"
#include "nema/event/event.h"
#include "nema/event/event_bus.h"

namespace nema {

void DummyBatteryDriver::onRegister(Runtime& rt) {
    rt_ = &rt;
    bus_ = &rt.events();
}

void DummyBatteryDriver::start() {
    if (!bus_) return;
    bus_->publish({events::BatteryChanged, {
        {"level", std::to_string(level_)},
        {"charging", "0"}}});
}

void DummyBatteryDriver::tick(uint64_t nowMs) {
    if (nowMs - lastTickMs_ < 30000) return;
    lastTickMs_ = nowMs;
    bus_->publish({events::BatteryChanged, {
        {"level", std::to_string(level_)},
        {"charging", charging_ ? "1" : "0"}}});
}

} // namespace nema
