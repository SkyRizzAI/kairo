#pragma once
#include "nema/hal/driver.h"
#include <cstdint>

namespace nema {

// The specific job a sensor does. Kept granular (mirrors the sensors.* caps) so
// apps/UI can request "the light sensor" etc. Extend as new parts appear.
enum class SensorType : uint8_t {
    Environment,   // temperature + humidity (e.g. AHT20)
    Light,         // ambient illuminance (e.g. LTR-303ALS)
    Motion,        // accelerometer (e.g. SC7A20)
    Pressure,      // barometric
    Proximity,
    Other,
};

inline const char* sensorTypeName(SensorType t) {
    switch (t) {
        case SensorType::Environment: return "Environment";
        case SensorType::Light:       return "Light";
        case SensorType::Motion:      return "Motion";
        case SensorType::Pressure:    return "Pressure";
        case SensorType::Proximity:   return "Proximity";
        default:                      return "Sensor";
    }
}

// ISensor — a physical sensor exposing one or more named, typed CHANNELS
// (Environment → Temp °C + Humidity %; Motion → X/Y/Z g; Light → lux). The
// channel model is generic so the settings UI and apps can display/read ANY
// sensor without knowing the specific part — while type() keeps it specific
// enough to map to a capability and to request the right sensor.
struct ISensor : IDriver {
    virtual const char* label() const = 0;          // "AHT20", "LTR-303ALS"
    virtual SensorType  type()  const = 0;
    virtual int         channelCount() const = 0;   // ≥1
    virtual const char* channelName(int i) const = 0;   // "Temp", "Humidity", "X"…
    virtual const char* channelUnit(int i) const = 0;   // "C", "%", "lx", "g"…

    // Sample every channel now (I²C read). Returns false on bus error; last
    // good values are retained. value(i) returns the most recent reading.
    virtual bool  read() = 0;
    virtual float value(int i) const = 0;
};

} // namespace nema
