#pragma once
#include "nema/hal/radio_wifi.h"
#include "nema/service.h"
#include "nema/thread.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

namespace nema {

class Runtime;

// SimWifiRadio — WASM/sim implementation of IRadioWifi (Plan 87 Fase 4).
//
// scan()        — delegates to the live IWifiDriver (SimWifiDriver), synthesises
//                 fake BSSIDs so the sim scan result looks realistic.
// deauthStart() — starts a sim loop thread that pushes "deauth_sent" events at
//                 ~10 Hz; the app consumes them via waitEvent().
// beaconSpamStart() — similar; pushes "beacon_sent" events for each fake SSID.
//
// The loop thread runs independently of the app WASM sandbox (same architecture
// as the ESP32 Core 0 native loop), keeping the UI responsive.
class SimWifiRadio : public IRadioWifi, public IService {
public:
    void init(Runtime& rt);

    // ── IRadioWifi ────────────────────────────────────────────────────────────
    std::vector<RadioScanResult> scan() override;
    bool deauthStart(std::string_view bssid, uint8_t channel) override;
    bool deauthStop()  override;
    bool beaconSpamStart(const std::vector<std::string>& ssids) override;
    bool beaconSpamStop() override;

    // ── IService ──────────────────────────────────────────────────────────────
    const char* name() const override { return "SimWifiRadio"; }
    void start() override {}
    void stop()  override;

private:
    static void loopEntry(void* self);
    void        loopFn();

    Runtime*  rt_          = nullptr;
    Thread    loopThread_;
    std::mutex               mu_;
    std::atomic<bool>        doDeauth_{false};
    std::atomic<bool>        doBeacon_{false};
    std::string              deauthBssid_;
    uint8_t                  deauthChannel_ = 0;
    std::vector<std::string> beaconSsids_;
};

} // namespace nema
