#include "nema/system/capability_registry.h"
#include "nema/event/event_bus.h"

namespace nema {

static const char* stateStr(ResourceState s) {
    switch (s) {
        case ResourceState::Available: return "available";
        case ResourceState::Fault:     return "fault";
        case ResourceState::Absent:    return "absent";
    }
    return "absent";
}

void CapabilityRegistry::add(const std::string& capability) {
    if (static_.insert(capability).second) {
        order_.push_back(capability);   // first time seen → keep ordered for list()
    }
}

bool CapabilityRegistry::has(const std::string& capability) const {
    return static_.count(capability) != 0;
}

const std::vector<std::string>& CapabilityRegistry::list() const {
    return order_;
}

void CapabilityRegistry::setState(const std::string& capability, ResourceState s) {
    auto it = live_.find(capability);
    if (it != live_.end() && it->second == s) return;  // no change → no event
    live_[capability] = s;
    if (bus_) {
        bus_->publish({events::ResourceChanged,
                       {{"resource", capability}, {"state", stateStr(s)}}});
    }
}

ResourceState CapabilityRegistry::stateOf(const std::string& capability) const {
    auto it = live_.find(capability);
    if (it != live_.end()) return it->second;
    // Static cap with no liveness report → Available; unknown cap → Absent.
    return has(capability) ? ResourceState::Available : ResourceState::Absent;
}

bool CapabilityRegistry::available(const std::string& capability) const {
    return has(capability) && stateOf(capability) == ResourceState::Available;
}

} // namespace nema
