#include "kairo/services/clock_service.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"

namespace kairo {

ClockService::ClockService(Logger& log, EventBus& bus)
    : log_(log), bus_(bus) {}

void ClockService::start() {
    log_.info("ClockService", "started");
}

void ClockService::stop() {
    log_.info("ClockService", "stopped");
}

void ClockService::tick(uint64_t nowMs) {
    if (nowMs - lastTickMs_ >= 1000) {
        lastTickMs_ = nowMs;
        bus_.publish({events::ClockTick, {{"uptimeMs", std::to_string(nowMs)}}});
    }
}

} // namespace kairo
