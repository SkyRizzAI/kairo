#include "nema/services/resource_broker.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/event/event.h"
#include "nema/event/event_bus.h"

namespace nema {

void ResourceBroker::init(Runtime& rt) {
    rt_ = &rt;

    // Auto-release on any app exit (clean or crash). AppHostExited is always
    // dispatched via AsyncEventPoster, so this subscriber runs on the main thread.
    rt.events().subscribe(events::AppHostExited, [this](const Event& e) {
        std::string appId;
        for (const auto& f : e.payload)
            if (std::string(f.key) == "id") appId = f.value;
        if (!appId.empty()) releaseAll(appId);
    });
}

NemaResult<uint32_t, LeaseError> ResourceBroker::acquire(const std::string& appId,
                                                          const std::string& cap) {
    std::lock_guard<std::mutex> g(mu_);

    auto it = leases_.find(cap);
    if (it != leases_.end()) {
        if (it->second.appId == appId)
            return {true, it->second.handle, {}};  // re-entrant: same owner
        LeaseError err{"busy", it->second.appId};
        return {false, 0u, err};
    }

    uint32_t handle = nextHandle_++;
    leases_[cap] = {appId, handle};
    byApp_[appId].push_back(cap);

    if (rt_) rt_->log().info("ResourceBroker", "acquired",
        {{"app", appId}, {"cap", cap}, {"handle", std::to_string(handle)}});

    return {true, handle, {}};
}

NemaResult<void, std::string> ResourceBroker::release(const std::string& appId,
                                                       uint32_t handle) {
    std::lock_guard<std::mutex> g(mu_);

    for (auto it = leases_.begin(); it != leases_.end(); ++it) {
        if (it->second.handle == handle && it->second.appId == appId) {
            const std::string cap = it->first;
            leases_.erase(it);

            auto& caps = byApp_[appId];
            caps.erase(std::remove(caps.begin(), caps.end(), cap), caps.end());
            if (caps.empty()) byApp_.erase(appId);

            if (rt_) rt_->log().info("ResourceBroker", "released",
                {{"app", appId}, {"cap", cap}});
            return {true, {}};
        }
    }
    return {false, "lease not found"};
}

bool ResourceBroker::holdsLease(const std::string& appId,
                                 const std::string& cap) const {
    std::lock_guard<std::mutex> g(mu_);
    auto it = leases_.find(cap);
    return it != leases_.end() && it->second.appId == appId;
}

void ResourceBroker::releaseAll(const std::string& appId) {
    std::lock_guard<std::mutex> g(mu_);

    auto byIt = byApp_.find(appId);
    if (byIt == byApp_.end()) return;

    size_t count = byIt->second.size();
    for (const auto& cap : byIt->second)
        leases_.erase(cap);
    byApp_.erase(byIt);

    if (rt_ && count > 0) rt_->log().info("ResourceBroker", "auto-released",
        {{"app", appId}, {"count", std::to_string(count)}});
}

} // namespace nema
