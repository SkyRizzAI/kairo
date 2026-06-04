#pragma once
#include <string>
#include <vector>

namespace kairo {

// Application code checks capabilities — never the board type.
// Example: capabilities.has("wifi") instead of isEsp32().
class CapabilityRegistry {
public:
    void add(std::string capability);
    bool has(const std::string& capability) const;
    const std::vector<std::string>& list() const;

private:
    std::vector<std::string> caps_;
};

} // namespace kairo
