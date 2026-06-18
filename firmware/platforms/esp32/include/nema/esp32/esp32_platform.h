#pragma once
#include "nema/platform.h"
#include "nema/esp32/esp32_clock.h"
#include "nema/esp32/esp32_wifi_driver.h"
#include "nema/esp32/esp32_http_client.h"
#include "nema/esp32/esp32_ble.h"
#include "nema/esp32/nvs_config_store.h"
#include "nema/esp32/esp32_usb_cdc.h"
#include "nema/esp32/esp32_usb_hid.h"
#include "nema/esp32/esp32_ota.h"
#include "nema/link/link_service.h"
#include "nema/link/ble_link_transport.h"
#include "nema/link/usb_cdc_link_transport.h"
#include "nema/link/mux_transport.h"
#include "nema/hal/remote_screen_tap.h"
#include "nema/services/remote_service.h"
#include "nema/services/cli_service.h"
#include "nema/services/profile_service.h"
#include "nema/fs/mem_filesystem.h"
#include "nema/fs/vfs.h"
#include "nema/esp32/littlefs_filesystem.h"

namespace nema {

// ESP32 platform — used by both Palanu Dev Board and future Palanu Board V1.
// Always Human output mode (Serial/UART).
class Esp32Platform : public IPlatform {
public:
    const char* name() const override { return "esp32"; }
    IClock& clock() override { return clock_; }
    OutputMode outputMode() const override { return OutputMode::Human; }
    void registerDrivers(Runtime& rt) override;
    void postRegister(Runtime& rt) override;   // wrap display + wire PLP-over-BLE
    void idle() override;   // vTaskDelay(5ms)
    void power(PowerAction action) override;   // esp_restart / esp_deep_sleep_start
    bool syncNtp() override;   // Plan 62 — SNTP client

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
    Esp32UsbHid      usbHid_;
    NvsConfigStore   config_;

    // PLP remote layer over BLE (Plan 35). Only wired when the board has a
    // display + BLE capability; screen-tap decorates the board's display driver.
    Runtime*           rt_ = nullptr;
    Esp32UsbCdc        usbCdc_;
    UsbCdcLinkTransport usbLink_;
    BleLinkTransport   cable_;
    MuxTransport       mux_;        // BLE + USB → one link (Plan 37)
    LinkService        link_;
    RemoteScreenTap    tap_;
    RemoteService      remote_;
    Esp32OtaUpdater    otaUpdater_;   // firmware OTA via esp_ota (Plan 39)
    CliService         cli_;
    ProfileService     profile_;   // owner identity (Plan 40)
    Vfs                vfs_;       // mount table
    LittleFsFileSystem rootFs_;    // "/"   — persistent (internal flash)
    MemFileSystem      tmpFs_;     // "/tmp" — volatile scratch (RAM)
    bool               remoteWired_ = false;
};

} // namespace nema
