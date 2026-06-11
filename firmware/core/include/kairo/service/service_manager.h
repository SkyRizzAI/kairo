#pragma once
#include "kairo/types.h"
#include <unordered_map>

namespace kairo {

class ServiceContainer;
class Logger;
class EventBus;
struct IService;

class ServiceManager {
public:
    ServiceManager(ServiceContainer& container, Logger& log, EventBus& bus);

    void startAll();
    void stopAll();
    void tickAll(uint64_t nowMs);

    // Dynamic lifecycle — for services installed/removed while the runtime is
    // already Running (e.g. a service shipped by an app, installed through the
    // AppRegistry). startOne is a no-op if the service is already Running.
    void startOne(IService* svc);
    void stopOne (IService* svc);

    ServiceState stateOf(IService* svc) const;

private:
    void transition(IService* svc, ServiceState to);

    ServiceContainer& container_;
    Logger&           log_;
    EventBus&         bus_;

    std::unordered_map<IService*, ServiceState> states_;
};

} // namespace kairo
