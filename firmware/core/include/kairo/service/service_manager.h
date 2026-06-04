#pragma once
#include "kairo/types.h"
#include <unordered_map>

namespace kairo {

class ServiceContainer;
class Logger;
class EventBus;
struct IClock;
struct IService;

class ServiceManager {
public:
    ServiceManager(ServiceContainer& container, Logger& log, EventBus& bus, IClock& clock);

    void startAll();
    void stopAll();
    void tickAll(uint64_t nowMs);

    ServiceState stateOf(IService* svc) const;

private:
    void transition(IService* svc, ServiceState to);

    ServiceContainer& container_;
    Logger&           log_;
    EventBus&         bus_;

    std::unordered_map<IService*, ServiceState> states_;
};

} // namespace kairo
