#pragma once
#include "nema/hal/wifi.h"
#include "nema/service.h"
#include <vector>
#include <cstdint>

namespace nema {

class Logger;
class AsyncEventPoster;
class CapabilityRegistry;

// Esp32WifiDriver — STA-mode WiFi for ESP32.
//
// The ESP-IDF WiFi handler runs in the 'sys_evt' FreeRTOS task (NOT the main
// Palanu loop task). Publishing to EventBus from there would race on dual-core.
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
    bool        isEnabled() const override { return enabled_; }
    void        setEnabled(bool on) override;
    bool        connect(const char* ssid, const char* password = "") override;
    bool        connectSaved(const char* ssid) override;
    void        disconnect() override;
    bool        isConnected() const override { return connected_; }
    const char* ssid()        const override { return ssid_; }

    WifiState state()     const override { return state_; }
    WifiError lastError() const override { return lastError_; }
    int8_t    rssi()      const override;

    void                            scan() override;   // blocking (worker thread)
    const std::vector<WifiNetwork>& scanResults() const override { return scan_; }
    const char*  ip()       const override { return ip_; }
    WifiIpConfig ipConfig() const override;
    void         setIpConfig(const WifiIpConfig&) override;

    // Saved networks (NVS-backed)
    void   saveNetwork(const char* ssid, const char* password) override;
    void   forgetNetwork(const char* ssid) override;
    void   setAutoJoin(const char* ssid, bool on) override;
    size_t savedCount() const override;
    bool   savedAt(size_t i, WifiProfile& out) const override;
    void   autoConnect() override;   // blocking (worker thread)

    // IService
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

    // ESP-IDF handler — sys_evt task; only touches poster_/flags (no EventBus)
    void onWifiEvent(int32_t event_id, void* event_data);

private:
    void setState(WifiState s, WifiError e = WifiError::None);  // updates + posts event
    void loadIpConfig();        // NVS → ipcfg_/staticIp_
    void saveIpConfig();        // ipcfg_ → NVS
    void applyStaticIp();       // push ipcfg_ static address onto esp_netif

    Logger*             log_      = nullptr;
    AsyncEventPoster*   poster_   = nullptr;
    CapabilityRegistry* caps_     = nullptr;
    bool              connected_  = false;
    bool              enabled_    = false;   // radio started (esp_wifi_start)
    WifiState         state_      = WifiState::Disabled;
    WifiError         lastError_  = WifiError::None;
    char              ssid_[33]   = {};
    char              pass_[65]   = {};
    char              ip_[16]     = "0.0.0.0";
    bool              staticIp_   = false;
    WifiIpConfig      ipcfg_;
    std::vector<WifiNetwork> scan_;
};

} // namespace nema
