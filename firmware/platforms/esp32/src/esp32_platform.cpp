#include "kairo/esp32/esp32_platform.h"
#include "kairo/runtime.h"
#include "kairo/config/config_store.h"
#include "kairo/service/service_container.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace kairo {

void Esp32Platform::registerDrivers(Runtime& rt) {
    // Each driver self-registers via its lifecycle hook (deps, service, caps, hw).
    wifi_.onRegister(rt);
    http_.onRegister(rt);

    // NVS-backed config store — available to all ESP32 boards.
    config_.init(rt.log());
    rt.container().registerService(&config_);
    rt.container().registerAs<IConfigStore>(&config_);
}

void Esp32Platform::idle() {
    vTaskDelay(pdMS_TO_TICKS(5));
}

} // namespace kairo
