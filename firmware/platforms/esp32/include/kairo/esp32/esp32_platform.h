#pragma once
#include "kairo/platform.h"
#include "kairo/esp32/esp32_clock.h"
#include "kairo/esp32/esp32_wifi_driver.h"
#include "kairo/esp32/esp32_http_client.h"

namespace kairo {

// ESP32 platform — used by both Kairo Dev Board and future Kairo Board V1.
// Always Human output mode (Serial/UART).
class Esp32Platform : public IPlatform {
public:
    const char* name() const override { return "esp32"; }
    IClock& clock() override { return clock_; }
    OutputMode outputMode() const override { return OutputMode::Human; }
    void registerDrivers(Runtime& rt) override;
    void idle() override;   // vTaskDelay(5ms)

    Esp32WifiDriver& wifi() { return wifi_; }

private:
    Esp32Clock       clock_;
    Esp32WifiDriver  wifi_;
    Esp32HttpClient  http_;
};

} // namespace kairo
