#pragma once
#include "nema/service.h"

namespace nema {

class Logger;
class EventBus;

class ClockService : public IService {
public:
    ClockService(Logger& log, EventBus& bus);

    const char* name() const override { return "ClockService"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t nowMs) override;

private:
    Logger&   log_;
    EventBus& bus_;
    uint64_t  lastTickMs_ = 0;
};

} // namespace nema
