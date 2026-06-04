#pragma once
#include "kairo/hal/driver.h"
#include <string>
#include <vector>

namespace kairo {

struct HardwareEntry {
    std::string id;       // "battery", "wifi"
    DriverKind  kind;
    std::string detail;
};

class HardwareRegistry {
public:
    void add(HardwareEntry entry);
    bool has(DriverKind kind) const;
    const std::vector<HardwareEntry>& list() const;

private:
    std::vector<HardwareEntry> entries_;
};

} // namespace kairo
