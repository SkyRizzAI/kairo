#include "nema/services/sensor_service.h"

namespace nema {

void SensorService::addSensor(ISensor* s, const char* id, const char* desc) {
    if (!s) return;
    sensors_.push_back({s, id ? id : "", desc ? desc : ""});
}

} // namespace nema
