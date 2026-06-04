#pragma once

namespace kairo {

class Runtime;
struct IClock;

struct IPlatform {
    enum class OutputMode { Human, Json };

    virtual ~IPlatform() = default;
    virtual const char* name() const = 0;
    virtual IClock& clock() = 0;
    virtual OutputMode outputMode() const { return OutputMode::Human; }
    // Register platform-specific drivers/services into the runtime.
    // Called during Runtime::registerServices(), after initCore().
    virtual void registerDrivers(Runtime& rt) = 0;
    // Called each loop iteration — platform I/O, stdin poll, etc.
    virtual void idle() {}
};

} // namespace kairo
