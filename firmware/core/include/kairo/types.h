#pragma once
#include <cstdint>
#include <string>

namespace kairo {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

enum class ServiceState : uint8_t {
    Created = 0,
    Starting,
    Running,
    Stopping,
    Stopped,
    Failed
};

enum class BootPhase : uint8_t {
    None = 0,
    PlatformLoaded,
    BoardLoaded,
    CoreReady,
    ServicesRegistered,
    Running
};

// Key-value pair used by Logger fields and Event payload
struct Field {
    const char* key;
    std::string value;
};

} // namespace kairo
