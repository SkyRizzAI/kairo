#pragma once

namespace nema {

class Runtime;
struct IClock;

struct IPlatform {
    enum class OutputMode { Human, Json };
    enum class PowerAction { Restart, Shutdown };

    virtual ~IPlatform() = default;
    virtual const char* name() const = 0;
    virtual IClock& clock() = 0;
    virtual OutputMode outputMode() const { return OutputMode::Human; }
    // Carry out a hardware power action. Default is a no-op: on the host/simulator
    // Runtime::requestRestart/Shutdown sets shutdownRequested_ and the run() loop
    // exits cleanly. On real hardware (e.g. ESP32) there is no run() loop to exit —
    // the platform overrides this to actually reboot/sleep the chip.
    virtual void power(PowerAction) {}
    // Register platform-specific drivers/services into the runtime.
    // Called during Runtime::registerServices(), after initCore().
    virtual void registerDrivers(Runtime& rt) = 0;
    // Called after the board has registered its hardware (describeHardware) but
    // before the Canvas binds to the display driver. Use this to decorate
    // board-provided drivers — e.g. wrap the display with a remote screen-tap.
    virtual void postRegister(Runtime& rt) { (void)rt; }
    // Called each loop iteration — platform I/O, stdin poll, etc.
    virtual void idle() {}
};

} // namespace nema
