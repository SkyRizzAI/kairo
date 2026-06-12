#pragma once
#include "nema/types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace nema {

struct LogEntry {
    uint64_t          epochMs;    // wall-clock timestamp
    LogLevel          level;
    const char*       component;  // e.g. "Runtime", "WifiService"
    std::string       message;
    std::vector<Field> fields;   // optional structured key-value pairs
};

} // namespace nema
