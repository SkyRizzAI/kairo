#include "nema/sim/sim_wifi_radio.h"
#include "nema/runtime.h"
#include "nema/hal/wifi.h"
#include "nema/service/service_container.h"
#include <cstdio>
#include <cstring>

namespace nema {

void SimWifiRadio::init(Runtime& rt) {
    rt_ = &rt;
    // Start the native loop thread (sim equivalent of ESP32 Core 0 task).
    loopThread_.start({"RadioLoop", 4096, 5, -1}, loopEntry, this);
}

void SimWifiRadio::stop() {
    doDeauth_ = false;
    doBeacon_ = false;
    loopThread_.requestStop();
    loopThread_.join();
}

std::vector<RadioScanResult> SimWifiRadio::scan() {
    auto* wifi = rt_->container().resolve<IWifiDriver>();
    if (!wifi) return {};

    // Reuse the cooked sim driver to get the router's network list.
    wifi->scan();  // blocks ~800ms (sim latency)

    const auto& nets = wifi->scanResults();
    std::vector<RadioScanResult> out;
    out.reserve(nets.size());

    int idx = 0;
    for (const auto& n : nets) {
        RadioScanResult r{};
        // Synthesise deterministic fake BSSIDs (not exposed by IWifiDriver).
        std::snprintf(r.bssid, sizeof(r.bssid),
                      "AA:BB:CC:DD:EE:%02X", (++idx) & 0xFF);
        std::strncpy(r.ssid,    n.ssid,  sizeof(r.ssid) - 1);
        r.channel = static_cast<uint8_t>(1 + (idx % 11));  // spread across 1-11
        r.rssi    = n.rssi;
        std::strncpy(r.auth, n.secured ? "wpa2" : "open", sizeof(r.auth) - 1);
        out.push_back(r);
    }
    return out;
}

bool SimWifiRadio::deauthStart(std::string_view bssid, uint8_t channel) {
    std::lock_guard<std::mutex> g(mu_);
    deauthBssid_   = std::string(bssid);
    deauthChannel_ = channel;
    doBeacon_      = false;
    doDeauth_      = true;
    return true;
}

bool SimWifiRadio::deauthStop() {
    doDeauth_ = false;
    return true;
}

bool SimWifiRadio::beaconSpamStart(const std::vector<std::string>& ssids) {
    std::lock_guard<std::mutex> g(mu_);
    beaconSsids_ = ssids;
    doDeauth_    = false;
    doBeacon_    = true;
    return true;
}

bool SimWifiRadio::beaconSpamStop() {
    doBeacon_ = false;
    return true;
}

void SimWifiRadio::loopEntry(void* self) {
    static_cast<SimWifiRadio*>(self)->loopFn();
}

void SimWifiRadio::loopFn() {
    char buf[160];

    while (!loopThread_.shouldStop()) {
        if (doDeauth_) {
            std::string bssid;
            uint8_t ch;
            {
                std::lock_guard<std::mutex> g(mu_);
                bssid = deauthBssid_;
                ch    = deauthChannel_;
            }
            std::snprintf(buf, sizeof(buf),
                          "{\"bssid\":\"%s\",\"channel\":%d}",
                          bssid.c_str(), static_cast<int>(ch));
            pushEvent("deauth_sent", buf);
            Thread::sleepMs(100);

        } else if (doBeacon_) {
            std::vector<std::string> ssids;
            {
                std::lock_guard<std::mutex> g(mu_);
                ssids = beaconSsids_;
            }
            for (const auto& s : ssids) {
                if (loopThread_.shouldStop() || !doBeacon_) break;
                std::snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\"}", s.c_str());
                pushEvent("beacon_sent", buf);
            }
            Thread::sleepMs(100);

        } else {
            Thread::sleepMs(10);
        }
    }
}

} // namespace nema
