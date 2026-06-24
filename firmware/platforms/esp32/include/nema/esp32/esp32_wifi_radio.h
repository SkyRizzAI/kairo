#pragma once
#include "nema/hal/radio_wifi.h"
#include "nema/service.h"
#include "nema/thread.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>

namespace nema {

// Esp32WifiRadio — ESP32 hardware implementation of IRadioWifi (Plan 87 Fase 4+5).
//
// scan()           — esp_wifi_scan_start (blocking) → returns real BSSIDs/RSSIs.
// deauthStart()    — spawns a FreeRTOS thread pinned to Core 0 that calls
//                    esp_wifi_80211_tx every 100ms with a crafted deauth frame.
// beaconSpamStart()— same, sends beacon frames for each fake SSID.
// monitorOpen()    — enables WiFi promiscuous mode; raw frames from the RX
//                    callback are pushed into the base-class ring buffer via
//                    pushFrame(). App drains frames with monitorRead().
// monitorClose()   — disables promiscuous mode.
// inject()         — esp_wifi_80211_tx (raw frame injection).
//
// The native loop (Core 0) is never in the app WASM sandbox (Core 1). UI stays
// responsive regardless of frame transmission rate.
class Esp32WifiRadio : public IRadioWifi, public IService {
public:
    void init(Runtime& rt);

    // ── IRadioWifi ────────────────────────────────────────────────────────────
    std::vector<RadioScanResult> scan() override;
    bool monitorOpen(uint8_t ch)  override;
    void monitorClose()           override;
    bool inject(uint8_t ch, const uint8_t* frame, size_t len) override;
    bool deauthStart(std::string_view bssid, uint8_t channel) override;
    bool deauthStop()  override;
    bool beaconSpamStart(const std::vector<std::string>& ssids) override;
    bool beaconSpamStop() override;
    bool probeFloodStart(std::string_view ssid, uint8_t channel) override;
    bool probeFloodStop() override;

    // ── IService ──────────────────────────────────────────────────────────────
    const char*  name() const override { return "Esp32WifiRadio"; }
    DriverKind   kind() const override { return DriverKind::Wifi; }
    void start() override {}
    void stop()  override;

private:
    static void loopEntry(void* self);
    void        deauthLoop();
    void        beaconLoop();
    void        probeLoop();

    // Parse "AA:BB:CC:DD:EE:FF" string → 6-byte array. Returns false on error.
    static bool parseBssid(std::string_view s, uint8_t out[6]);

    Runtime*  rt_          = nullptr;
    Thread    loopThread_;
    std::mutex               mu_;
    std::atomic<bool>        doDeauth_{false};
    std::atomic<bool>        doBeacon_{false};
    std::atomic<bool>        doProbe_{false};
    uint8_t                  deauthBssid_[6]  = {};
    uint8_t                  deauthChannel_   = 0;
    std::vector<std::string> beaconSsids_;
    std::string              probeSsid_;
    uint8_t                  probeChannel_    = 1;
};

} // namespace nema
