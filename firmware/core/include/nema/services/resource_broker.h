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
// (Plan 87 Fase 2).
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
// Thread-safety: acquire() / release() / holdsLease() are safe from any thread
// (app threads). releaseAll() is called from the main-thread EventBus subscriber.
// All accesses are protected by mu_.
class ResourceBroker : public IService {
public:
    void init(Runtime& rt);

    // ── App-thread API ───────────────────────────────────────────────────────

    // Acquire an exclusive lease for `cap` on behalf of `appId`.
    // Returns {ok, handle} if granted; {err, LeaseError{busy, owner}} if busy;
    // re-entrant: same appId acquiring the same cap returns the existing handle.
    NemaResult<uint32_t, LeaseError> acquire(const std::string& appId,
                                              const std::string& cap);

    // Release the lease identified by (appId, handle).
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

    struct LeaseRecord {
        std::string appId;
        uint32_t    handle = 0;
    };

    mutable std::mutex mu_;
    uint32_t nextHandle_ = 1;

    std::unordered_map<std::string, LeaseRecord>          leases_;  // cap → record
    std::unordered_map<std::string, std::vector<std::string>> byApp_;  // appId → caps

    Runtime* rt_ = nullptr;
};

} // namespace nema
