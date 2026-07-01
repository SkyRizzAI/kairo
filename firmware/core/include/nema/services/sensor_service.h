#pragma once
#include "nema/hal/sensor.h"
#include <string>
#include <vector>

namespace nema {

// SensorService — the runtime sensor registry (`rt.sensors()`).
//
// Multi-instance like AudioService/LedService: a board registers each physical
// sensor (addSensor) and the settings UI / apps enumerate them and read their
// generic channels. Pure registry — sampling is on-demand via ISensor::read(),
// so there's no background polling cost until something asks.
class SensorService {
public:
    void        addSensor(ISensor* s, const char* id, const char* desc);
    int         count() const { return (int)sensors_.size(); }
    ISensor*    sensor(int i)       { return (i >= 0 && i < count()) ? sensors_[(size_t)i].s : nullptr; }
    const char* id(int i)   const   { return (i >= 0 && i < count()) ? sensors_[(size_t)i].id.c_str()   : ""; }
    const char* desc(int i) const   { return (i >= 0 && i < count()) ? sensors_[(size_t)i].desc.c_str() : ""; }

private:
    struct Entry { ISensor* s; std::string id; std::string desc; };
    std::vector<Entry> sensors_;
};

} // namespace nema
