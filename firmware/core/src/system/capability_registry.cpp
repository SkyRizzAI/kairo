#include "kairo/system/capability_registry.h"
#include <algorithm>

namespace kairo {

void CapabilityRegistry::add(std::string capability) {
    caps_.push_back(std::move(capability));
}

bool CapabilityRegistry::has(const std::string& capability) const {
    return std::find(caps_.begin(), caps_.end(), capability) != caps_.end();
}

const std::vector<std::string>& CapabilityRegistry::list() const {
    return caps_;
}

} // namespace kairo
