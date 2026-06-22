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

// Esp32WifiRadio — ESP32 hardware implementation of IRadioWifi (Plan 87 Fase 4).
//
// scan()         — esp_wifi_scan_start (blocking) → returns real BSSIDs/RSSIs.
// deauthStart()  — spawns a FreeRTOS thread pinned to Core 0 that calls
//                  esp_wifi_80211_tx every 100ms with a crafted deauth frame.
// beaconSpamStart() — same, sends beacon frames for each fake SSID.
//
// The native loop (Core 0) is never in the app WASM sandbox (Core 1). UI stays
// responsive regardless of frame transmission rate.
//
// Requires net.wifi.inject lease (enforced by host gating prologue generated
// from @lease annotation in api/wifi.pidl).
class Esp32WifiRadio : public IRadioWifi, public IService {
public:
    void init(Runtime& rt);

    // ── IRadioWifi ────────────────────────────────────────────────────────────
    std::vector<RadioScanResult> scan() override;
    bool deauthStart(std::string_view bssid, uint8_t channel) override;
    bool deauthStop()  override;
    bool beaconSpamStart(const std::vector<std::string>& ssids) override;
    bool beaconSpamStop() override;

    // ── IService ──────────────────────────────────────────────────────────────
    const char* name() const override { return "Esp32WifiRadio"; }
    void start() override {}
    void stop()  override;

private:
    static void loopEntry(void* self);
    void        deauthLoop();
    void        beaconLoop();

    // Parse "AA:BB:CC:DD:EE:FF" string → 6-byte array. Returns false on error.
    static bool parseBssid(std::string_view s, uint8_t out[6]);

    Runtime*  rt_          = nullptr;
    Thread    loopThread_;
    std::mutex               mu_;
    std::atomic<bool>        doDeauth_{false};
    std::atomic<bool>        doBeacon_{false};
    uint8_t                  deauthBssid_[6]  = {};
    uint8_t                  deauthChannel_   = 0;
    std::vector<std::string> beaconSsids_;
};

} // namespace nema
