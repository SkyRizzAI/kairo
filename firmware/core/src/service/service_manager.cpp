#include "kairo/service/service_manager.h"
#include "kairo/service/service_container.h"
#include "kairo/service.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"
#include <vector>
#include <algorithm>

namespace kairo {

static const char* stateStr(ServiceState s) {
    switch (s) {
        case ServiceState::Created:  return "Created";
        case ServiceState::Starting: return "Starting";
        case ServiceState::Running:  return "Running";
        case ServiceState::Stopping: return "Stopping";
        case ServiceState::Stopped:  return "Stopped";
        case ServiceState::Failed:   return "Failed";
    }
    return "?";
}

ServiceManager::ServiceManager(ServiceContainer& c, Logger& log, EventBus& bus)
    : container_(c), log_(log), bus_(bus) {
    for (auto* svc : c.services()) {
        states_[svc] = ServiceState::Created;
    }
}

void ServiceManager::transition(IService* svc, ServiceState to) {
    states_[svc] = to;
    log_.debug("ServiceManager", std::string(svc->name()) + " → " + stateStr(to));

    const char* evtName = nullptr;
    if (to == ServiceState::Running) evtName = events::ServiceStarted;
    else if (to == ServiceState::Stopped) evtName = events::ServiceStopped;
    else if (to == ServiceState::Failed)  evtName = events::ServiceFailed;

    if (evtName) {
        bus_.publish({evtName, {{"name", svc->name()}}});
    }
}

void ServiceManager::startOne(IService* svc) {
    if (states_[svc] == ServiceState::Running) return;
    transition(svc, ServiceState::Starting);
    try {
        svc->start();
        transition(svc, ServiceState::Running);
    } catch (const std::exception& e) {
        log_.error("ServiceManager",
            std::string(svc->name()) + " failed to start: " + e.what());
        transition(svc, ServiceState::Failed);
    } catch (...) {
        log_.error("ServiceManager",
            std::string(svc->name()) + " failed to start: unknown exception");
        transition(svc, ServiceState::Failed);
    }
}

void ServiceManager::stopOne(IService* svc) {
    if (states_[svc] != ServiceState::Running) return;
    transition(svc, ServiceState::Stopping);
    try {
        svc->stop();
    } catch (...) {}
    transition(svc, ServiceState::Stopped);
}

void ServiceManager::startAll() {
    for (auto* svc : container_.services()) {
        startOne(svc);
    }
}

void ServiceManager::stopAll() {
    auto svcs = container_.services();
    std::reverse(svcs.begin(), svcs.end());  // stop in reverse registration order
    for (auto* svc : svcs) {
        stopOne(svc);
    }
}

void ServiceManager::tickAll(uint64_t nowMs) {
    for (auto* svc : container_.services()) {
        if (states_[svc] == ServiceState::Running) {
            svc->tick(nowMs);
        }
    }
}

ServiceState ServiceManager::stateOf(IService* svc) const {
    auto it = states_.find(svc);
    return (it != states_.end()) ? it->second : ServiceState::Created;
}

} // namespace kairo
