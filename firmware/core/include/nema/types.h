#pragma once
#include <cstdint>
#include <string>

namespace nema {

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

// Dynamic liveness of a resource (Plan 42). Distinct from a *capability*
// (static "this box can do X"): a resource that exists can still be Absent
// (detached / not yet up) or Fault (init failed / crashed). A static
// capability that never reports liveness is treated as Available.
enum class ResourceState : uint8_t {
    Absent = 0,   // not present / detached / torn down
    Available,    // up and usable right now
    Fault         // present but failed (init error / crash)
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

} // namespace nema
