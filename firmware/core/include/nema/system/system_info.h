#pragma once
#include <string>
#include <cstdint>

namespace nema {

struct SystemInfo {
    std::string buildVersion    = "unknown";
    std::string firmwareVersion = "unknown";
    std::string platformName;
    std::string boardName;
    // On host these are placeholder zeros; filled by platform on real hardware.
    uint32_t cpuMhz  = 0;
    uint32_t ramKb   = 0;
    uint32_t psramKb = 0;
    uint32_t flashKb = 0;
};

} // namespace nema
