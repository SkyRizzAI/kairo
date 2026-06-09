#pragma once
#include "kairo/platform.h"
#include "kairo/esp32/esp32_clock.h"
#include "kairo/esp32/esp32_wifi_driver.h"
#include "kairo/esp32/esp32_http_client.h"
#include "kairo/esp32/esp32_ble.h"
#include "kairo/esp32/nvs_config_store.h"
#include "kairo/esp32/esp32_usb_cdc.h"
#include "kairo/link/link_service.h"
#include "kairo/link/ble_link_transport.h"
#include "kairo/link/usb_cdc_link_transport.h"
#include "kairo/link/mux_transport.h"
#include "kairo/hal/remote_screen_tap.h"
#include "kairo/services/remote_service.h"

namespace kairo {

// ESP32 platform — used by both Kairo Dev Board and future Kairo Board V1.
// Always Human output mode (Serial/UART).
class Esp32Platform : public IPlatform {
public:
    const char* name() const override { return "esp32"; }
    IClock& clock() override { return clock_; }
    OutputMode outputMode() const override { return OutputMode::Human; }
    void registerDrivers(Runtime& rt) override;
    void postRegister(Runtime& rt) override;   // wrap display + wire KLP-over-BLE
    void idle() override;   // vTaskDelay(5ms)

    Esp32WifiDriver& wifi() { return wifi_; }
    Esp32Ble&        ble()  { return ble_; }

private:
    static void powerThunk(void* user, uint8_t op);
    static void controlThunk(void* user, uint8_t op, const uint8_t* data, size_t len);
    static void readyThunk(void* user);

    Esp32Clock       clock_;
    Esp32WifiDriver  wifi_;
    Esp32HttpClient  http_;
    Esp32Ble         ble_;
    NvsConfigStore   config_;

    // KLP remote layer over BLE (Plan 35). Only wired when the board has a
    // display + BLE capability; screen-tap decorates the board's display driver.
    Runtime*           rt_ = nullptr;
    Esp32UsbCdc        usbCdc_;
    UsbCdcLinkTransport usbLink_;
    BleLinkTransport   cable_;
    MuxTransport       mux_;        // BLE + USB → one link (Plan 37)
    LinkService        link_;
    RemoteScreenTap    tap_;
    RemoteService      remote_;
    bool               remoteWired_ = false;
};

} // namespace kairo
