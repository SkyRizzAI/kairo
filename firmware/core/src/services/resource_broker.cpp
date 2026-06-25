#include "nema/services/resource_broker.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/event/event.h"
#include "nema/event/event_bus.h"
#include <algorithm>

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

void ResourceBroker::addExclusivityGroup(const std::string& id,
                                          std::vector<std::string> caps,
                                          const std::string& yieldableOwner) {
    std::lock_guard<std::mutex> g(mu_);
    ExclusivityGroup grp;
    grp.id             = id;
    grp.caps           = std::move(caps);
    grp.yieldableOwner = yieldableOwner;
    groups_.push_back(std::move(grp));
}

int ResourceBroker::findGroupIndex(const std::string& cap) const {
    for (int i = 0; i < (int)groups_.size(); ++i)
        for (const auto& c : groups_[i].caps)
            if (c == cap) return i;
    return -1;
}

NemaResult<uint32_t, LeaseError> ResourceBroker::acquire(const std::string& appId,
                                                          const std::string& cap) {
    // Collected outside the lock so we can post events after releasing mu_.
    std::string suspendedCap;
    std::string suspendedGroup;
    NemaResult<uint32_t, LeaseError> result{false, 0u, {}};

    {
        std::lock_guard<std::mutex> g(mu_);

        auto it = leases_.find(cap);
        if (it != leases_.end()) {
            if (it->second.appId == appId)
                return {true, it->second.handle, {}};  // re-entrant: same owner
            LeaseError err{"busy", it->second.appId};
            return {false, 0u, err};
        }

        // Exclusivity group: check for conflicting sibling caps.
        int gi = findGroupIndex(cap);
        if (gi >= 0) {
            auto& grp = groups_[gi];
            for (const auto& sibling : grp.caps) {
                if (sibling == cap) continue;
                auto sit = leases_.find(sibling);
                if (sit == leases_.end()) continue;
                const std::string& holder = sit->second.appId;
                if (holder == appId) {
                    // Same app already holds a sibling cap. A radio-takeover app
                    // (e.g. WiFi Marauder) legitimately owns inject AND monitor at
                    // once for the whole session — that's not a conflict with
                    // itself. Let it acquire the additional sibling.
                    continue;
                }
                if (holder == grp.yieldableOwner) {
                    // Auto-yield the system lease so the app can proceed.
                    auto& sibCaps = byApp_[holder];
                    sibCaps.erase(std::remove(sibCaps.begin(), sibCaps.end(), sibling),
                                  sibCaps.end());
                    if (sibCaps.empty()) byApp_.erase(holder);
                    leases_.erase(sit);
                    grp.suspendedCap = sibling;
                    suspendedCap     = sibling;
                    suspendedGroup   = grp.id;
                    break;  // only one yieldable holder expected per group
                } else {
                    // Non-yieldable sibling already holds the group → busy.
                    return {false, 0u, LeaseError{"busy", holder}};
                }
            }
            // Only count exclusive (non-yieldable) holders. The yieldable owner's
            // lease is auto-yielded on conflict (above) WITHOUT decrementing, so
            // counting it here would leave a phantom that never drops to 0 — which
            // blocks ResourceRestored after the app exits and makes the WiFi
            // "Radio in use by" banner persist. Decrements (release/releaseAll)
            // are already guarded by the same `!= yieldableOwner` condition.
            if (appId != grp.yieldableOwner)
                grp.exclusiveCount++;
        }

        uint32_t handle = nextHandle_++;
        leases_[cap] = {appId, handle};
        byApp_[appId].push_back(cap);
        result = {true, handle, {}};
    }

    if (rt_) {
        if (!suspendedCap.empty())
            rt_->asyncPoster().post({events::ResourceSuspended,
                {{"cap", suspendedCap}, {"group", suspendedGroup}, {"by", appId}}});
        rt_->log().info("ResourceBroker", "acquired",
            {{"app", appId}, {"cap", cap}});
    }

    return result;
}

NemaResult<void, std::string> ResourceBroker::release(const std::string& appId,
                                                       uint32_t handle) {
    std::string restoredCap;
    std::string restoredGroup;
    bool found = false;

    {
        std::lock_guard<std::mutex> g(mu_);

        for (auto it = leases_.begin(); it != leases_.end(); ++it) {
            if (it->second.handle == handle && it->second.appId == appId) {
                const std::string cap = it->first;
                leases_.erase(it);

                auto& caps = byApp_[appId];
                caps.erase(std::remove(caps.begin(), caps.end(), cap), caps.end());
                if (caps.empty()) byApp_.erase(appId);

                // Group cleanup: decrement exclusive count; emit Restored if last.
                int gi = findGroupIndex(cap);
                if (gi >= 0) {
                    auto& grp = groups_[gi];
                    if (appId != grp.yieldableOwner) {
                        grp.exclusiveCount = grp.exclusiveCount > 0
                                             ? grp.exclusiveCount - 1 : 0;
                        if (grp.exclusiveCount == 0 && !grp.suspendedCap.empty()) {
                            restoredCap   = grp.suspendedCap;
                            restoredGroup = grp.id;
                            grp.suspendedCap.clear();
                        }
                    }
                }

                found = true;
                break;
            }
        }
    }

    if (rt_ && found) {
        rt_->log().info("ResourceBroker", "released", {{"app", appId}});
        if (!restoredCap.empty())
            rt_->asyncPoster().post({events::ResourceRestored,
                {{"cap", restoredCap}, {"group", restoredGroup}}});
    }

    return found ? NemaResult<void, std::string>{true, {}}
                 : NemaResult<void, std::string>{false, "lease not found"};
}

bool ResourceBroker::holdsLease(const std::string& appId,
                                 const std::string& cap) const {
    std::lock_guard<std::mutex> g(mu_);
    auto it = leases_.find(cap);
    return it != leases_.end() && it->second.appId == appId;
}

void ResourceBroker::releaseAll(const std::string& appId) {
    std::vector<std::pair<std::string, std::string>> restored;  // (cap, group)
    size_t count = 0;

    {
        std::lock_guard<std::mutex> g(mu_);

        auto byIt = byApp_.find(appId);
        if (byIt == byApp_.end()) return;
        count = byIt->second.size();

        for (const auto& cap : byIt->second) {
            leases_.erase(cap);

            int gi = findGroupIndex(cap);
            if (gi >= 0) {
                auto& grp = groups_[gi];
                if (appId != grp.yieldableOwner) {
                    grp.exclusiveCount = grp.exclusiveCount > 0
                                         ? grp.exclusiveCount - 1 : 0;
                    if (grp.exclusiveCount == 0 && !grp.suspendedCap.empty()) {
                        restored.push_back({grp.suspendedCap, grp.id});
                        grp.suspendedCap.clear();
                    }
                }
            }
        }
        byApp_.erase(byIt);
    }

    if (rt_) {
        if (count > 0) rt_->log().info("ResourceBroker", "auto-released",
            {{"app", appId}, {"count", std::to_string(count)}});
        for (const auto& [cap, grp] : restored)
            rt_->asyncPoster().post({events::ResourceRestored,
                {{"cap", cap}, {"group", grp}}});
    }
}

} // namespace nema
