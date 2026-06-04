#include "kairo/esp32/esp32_platform.h"
#include "kairo/runtime.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace kairo {

void Esp32Platform::registerDrivers(Runtime& rt) {
    // Each driver self-registers via its lifecycle hook (deps, service, caps, hw).
    wifi_.onRegister(rt);
    http_.onRegister(rt);
    // Display + buttons registered by the Board layer (plan 17)
}

void Esp32Platform::idle() {
    vTaskDelay(pdMS_TO_TICKS(5));
}

} // namespace kairo
