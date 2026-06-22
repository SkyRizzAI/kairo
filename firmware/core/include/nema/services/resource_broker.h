#pragma once
#include "nema/service.h"
#include "host/nema_api.gen.h"  // LeaseError, NemaResult
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace nema {

class Runtime;

// ResourceBroker — exclusive single-owner leases for sensitive capabilities
// (Plan 87 Fase 2 + 3).
//
// Each capability may be held by at most one app at a time. When app A holds a
// lease for "net.wifi.monitor" and app B calls acquire("net.wifi.monitor"), the
// broker returns LeaseError{code:"busy", owner:appA.id}.
//
// Lease handles are opaque uint32_t issued monotonically. Handle 0 is always
// invalid (never issued). release() matches on (appId + handle) so an app cannot
// release another app's lease even if it learns the handle value.
//
// Auto-release on exit: init() subscribes to AppHostExited (emitted on both
// clean exit and crash via AsyncEventPoster, so it always fires on the main
// thread). This guarantees leases are freed even if the app forgot to call
// release() or crashed mid-operation.
//
// Exclusivity groups (Fase 3): caps that share one physical resource (e.g. the
// WiFi radio) can be registered as a group. The group may name one "yieldable"
// appId (e.g. "system:wifi") that is auto-preempted when an app acquires any
// sibling cap — the broker releases the system lease automatically and emits
// ResourceSuspended. When the last exclusive holder releases, ResourceRestored
// is emitted so the system service can reconnect.
//
// Thread-safety: acquire() / release() / holdsLease() are safe from any thread.
// releaseAll() is called from the main-thread EventBus subscriber. All accesses
// are protected by mu_.
class ResourceBroker : public IService {
public:
    void init(Runtime& rt);

    // ── Exclusivity groups (Fase 3) ──────────────────────────────────────────

    // Register a set of caps that share one physical resource. The `yieldableOwner`
    // appId (e.g. "system:wifi") is automatically preempted when any other appId
    // acquires a sibling cap — ResourceSuspended is emitted. When the last exclusive
    // holder releases, ResourceRestored is emitted.
    // Must be called before any acquire(). Thread-safe.
    void addExclusivityGroup(const std::string& id,
                              std::vector<std::string> caps,
                              const std::string& yieldableOwner);

    // ── App-thread API ───────────────────────────────────────────────────────

    // Acquire an exclusive lease for `cap` on behalf of `appId`.
    // Returns {ok, handle} if granted; {err, LeaseError{busy, owner}} if busy;
    // re-entrant: same appId acquiring the same cap returns the existing handle.
    // If cap is in an exclusivity group and its yieldableOwner holds a sibling
    // cap, the sibling is auto-released and ResourceSuspended is emitted.
    NemaResult<uint32_t, LeaseError> acquire(const std::string& appId,
                                              const std::string& cap);

    // Release the lease identified by (appId, handle).
    // If this was the last exclusive holder in an exclusivity group, emits
    // ResourceRestored so the system service can reclaim the resource.
    NemaResult<void, std::string> release(const std::string& appId, uint32_t handle);

    // ── Gating API ───────────────────────────────────────────────────────────

    // Returns true if appId currently holds any lease for cap.
    bool holdsLease(const std::string& appId, const std::string& cap) const;

    // ── IService ─────────────────────────────────────────────────────────────
    const char* name() const override { return "ResourceBroker"; }
    void start() override {}
    void stop()  override {}

private:
    // Release all leases held by appId (called from AppHostExited subscriber).
    void releaseAll(const std::string& appId);

    // Returns index into groups_ for the cap, or -1. Caller must hold mu_.
    int findGroupIndex(const std::string& cap) const;

    struct LeaseRecord {
        std::string appId;
        uint32_t    handle = 0;
    };

    // Mutual-exclusion group for caps that share one physical resource.
    struct ExclusivityGroup {
        std::string              id;
        std::vector<std::string> caps;
        std::string              yieldableOwner; // auto-preempted by any other app
        // Runtime state (protected by mu_):
        std::string              suspendedCap;   // cap auto-released when excl. started
        int                      exclusiveCount = 0; // non-yieldable holders in this group
    };

    mutable std::mutex mu_;
    uint32_t nextHandle_ = 1;

    std::unordered_map<std::string, LeaseRecord>             leases_;  // cap → record
    std::unordered_map<std::string, std::vector<std::string>> byApp_;  // appId → caps
    std::vector<ExclusivityGroup>                             groups_;

    Runtime* rt_ = nullptr;
};

} // namespace nema
