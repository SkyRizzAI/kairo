#pragma once

namespace kairo {

enum class DriverKind {
    Battery, Wifi, Bluetooth, Display, Storage, Other
};

class Runtime;  // forward declare — driver.h must not pull in all of runtime.h

struct IDriver {
    virtual ~IDriver() = default;
    virtual const char* name() const = 0;
    virtual DriverKind  kind() const = 0;

    // Lifecycle hook — called when driver is registered with the system.
    // Override to: capture dependencies (log, poster), register capabilities,
    // add self to service container, add hardware metadata.
    // Default: no-op (backward compatible).
    virtual void onRegister(Runtime& /*rt*/) {}
};

} // namespace kairo
