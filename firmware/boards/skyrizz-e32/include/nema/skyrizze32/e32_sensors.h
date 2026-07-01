#pragma once
#include "nema/hal/sensor.h"
#include <cstdint>

namespace nema::skyrizze32 {

// SkyRizz E32 on-board sensors on the shared I²C bus. Each driver probes at
// begin() and reports present() so the board only registers what actually ACKs.
//
// Bring-up note: register sequences follow the datasheet defaults but are
// UNVERIFIED on hardware. AHT20 (env) lives at 0x38 which clashes with the
// FT6336U touch controller on this board, so it is intentionally NOT registered
// here (its class is kept for boards without that clash). Light (LTR-303ALS
// @0x29) and Motion (SC7A20 @0x19) are conflict-free.

// LTR-303ALS ambient light sensor (@0x29). One channel: illuminance (approx lux
// from CH0; a precise value needs the CH0/CH1 ratio formula — TODO at bring-up).
class Ltr303 : public ISensor {
public:
    bool begin();
    bool present() const { return present_; }

    const char* name()  const override { return "LTR-303ALS"; }
    DriverKind  kind()  const override { return DriverKind::Other; }
    const char* label() const override { return "LTR-303ALS"; }
    SensorType  type()  const override { return SensorType::Light; }
    int         channelCount() const override { return 1; }
    const char* channelName(int) const override { return "Light"; }
    const char* channelUnit(int) const override { return "lx"; }
    bool        read() override;
    float       value(int) const override { return lux_; }

private:
    bool  present_ = false;
    float lux_ = 0.f;
};

// SC7A20 3-axis accelerometer (@0x19, LIS2DH-compatible). Channels X/Y/Z in g.
class Sc7a20 : public ISensor {
public:
    bool begin();
    bool present() const { return present_; }

    const char* name()  const override { return "SC7A20"; }
    DriverKind  kind()  const override { return DriverKind::Other; }
    const char* label() const override { return "SC7A20"; }
    SensorType  type()  const override { return SensorType::Motion; }
    int         channelCount() const override { return 3; }
    const char* channelName(int i) const override { return i == 0 ? "X" : i == 1 ? "Y" : "Z"; }
    const char* channelUnit(int) const override { return "g"; }
    bool        read() override;
    float       value(int i) const override { return i == 0 ? x_ : i == 1 ? y_ : z_; }

private:
    bool  present_ = false;
    float x_ = 0.f, y_ = 0.f, z_ = 0.f;
};

} // namespace nema::skyrizze32
