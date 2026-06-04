#pragma once
#include "kairo/hal/wifi.h"
#include "kairo/service.h"
#include <vector>
#include <cstdint>

namespace kairo {

class Logger;
class AsyncEventPoster;

// Esp32WifiDriver — STA-mode WiFi for ESP32.
//
// The ESP-IDF WiFi handler runs in the 'sys_evt' FreeRTOS task (NOT the main
// Kairo loop task). Publishing to EventBus from there would race on dual-core.
// Fix is generic: onWifiEvent() calls AsyncEventPoster::post(); Runtime drains
// the poster into EventBus from the main task each frame.
//
// scan()/connect() block and are called from a TaskRunner worker thread (Nema).
class Esp32WifiDriver : public IWifiDriver, public IService {
public:
    const char* name() const override { return "Esp32WifiDriver"; }
    DriverKind  kind() const override { return DriverKind::Wifi; }

    void onRegister(Runtime& rt) override;   // capture deps + self-register

    // IWifiDriver
    bool        connect(const char* ssid, const char* password = "") override;
    void        disconnect() override;
    bool        isConnected() const override { return connected_; }
    const char* ssid()        const override { return ssid_; }

    void                            scan() override;   // blocking (worker thread)
    const std::vector<WifiNetwork>& scanResults() const override { return scan_; }
    const char*  ip()       const override { return ip_; }
    WifiIpConfig ipConfig() const override;
    void         setIpConfig(const WifiIpConfig&) override;

    // IService
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

    // ESP-IDF handler — sys_evt task; only touches poster_/flags (no EventBus)
    void onWifiEvent(int32_t event_id, void* event_data);

private:
    void loadCredentials();   // NVS → ssid_/pass_, attempt reconnect
    void saveCredentials();   // ssid_/pass_ → NVS

    Logger*           log_        = nullptr;
    AsyncEventPoster* poster_     = nullptr;
    bool              connected_  = false;
    bool              connecting_ = false;
    char              ssid_[33]   = {};
    char              pass_[65]   = {};
    char              ip_[16]     = "0.0.0.0";
    bool              staticIp_   = false;
    WifiIpConfig      ipcfg_;
    std::vector<WifiNetwork> scan_;
};

} // namespace kairo
