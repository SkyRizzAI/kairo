#include "nema/system/hardware_registry.h"
#include <algorithm>

namespace nema {

void HardwareRegistry::add(HardwareEntry entry) {
    entries_.push_back(std::move(entry));
}

bool HardwareRegistry::has(DriverKind kind) const {
    return std::any_of(entries_.begin(), entries_.end(),
        [kind](const HardwareEntry& e) { return e.kind == kind; });
}

const std::vector<HardwareEntry>& HardwareRegistry::list() const {
    return entries_;
}

} // namespace nema
