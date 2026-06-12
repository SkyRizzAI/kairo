#include "nema/services/clock_service.h"
#include "nema/log/logger.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"

namespace nema {

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

} // namespace nema
