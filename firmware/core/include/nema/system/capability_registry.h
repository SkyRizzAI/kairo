#pragma once
#include "nema/types.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nema {

class EventBus;

// Two-axis capability model (Plan 42).
//
//   Axis 1 — STATIC capability (inventory): "this box can do X, ever."
//     add()/has()/list(). Append-only, idempotent, never removed.
//
//   Axis 2 — DYNAMIC liveness: "X is up and usable right now."
//     setState()/stateOf()/available(). Only the resource's OWNING service
//     calls setState(); doing so also publishes events::ResourceChanged.
//
// A static capability that never reports liveness is treated as Available
// (soldered hardware is "up" by default — no regression for existing caps).
//
// Application code checks capabilities — never the board type.
// Example: capabilities.has(caps::NetWifi) instead of isEsp32().
class CapabilityRegistry {
public:
    // Wire the bus so setState() can publish liveness events (set in initCore).
    void setBus(EventBus* bus) { bus_ = bus; }

    // --- Axis 1: static inventory ---
    void add(const std::string& capability);      // idempotent (dedup)
    bool has(const std::string& capability) const;
    const std::vector<std::string>& list() const; // insertion order, deduped

    // --- Axis 2: dynamic liveness ---
    void          setState(const std::string& capability, ResourceState s); // owner-only
    ResourceState stateOf(const std::string& capability) const;
    bool          available(const std::string& capability) const;

private:
    std::vector<std::string>                       order_; // for list()
    std::unordered_set<std::string>                static_;
    std::unordered_map<std::string, ResourceState> live_;
    EventBus*                                      bus_ = nullptr;
};

} // namespace nema
