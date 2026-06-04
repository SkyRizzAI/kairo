#include "kairo/sim/sim_battery_driver.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"

namespace kairo {

void SimBatteryDriver::init(Logger& log, EventBus& events) {
    log_    = &log;
    events_ = &events;
}

void SimBatteryDriver::start() {
    log_->info("SimBatteryDriver", "started", {{"level", std::to_string(level_)}});
}

void SimBatteryDriver::stop() {
    log_->info("SimBatteryDriver", "stopped");
}

void SimBatteryDriver::tick(uint64_t nowMs) {
    if (lastDrainMs_ == 0) { lastDrainMs_ = nowMs; return; }
    if (nowMs - lastDrainMs_ >= 5000) {
        lastDrainMs_ = nowMs;
        if (level_ > 0) level_--;
        log_->debug("SimBatteryDriver", "battery tick", {{"level", std::to_string(level_)}});
        events_->publish({events::BatteryChanged, {{"level", std::to_string(level_)}}});
    }
}

} // namespace kairo
