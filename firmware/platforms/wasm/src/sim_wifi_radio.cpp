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
    doMonitor_ = false;
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

bool SimWifiRadio::monitorOpen(uint8_t ch) {
    monitorChannel_ = ch;
    doMonitor_ = true;
    return true;
}

void SimWifiRadio::monitorClose() {
    doMonitor_ = false;
}

bool SimWifiRadio::inject(uint8_t /*ch*/, const uint8_t* /*frame*/, size_t /*len*/) {
    // Sim has no real radio — injection is a no-op success. (The matured-event
    // ring was removed in Plan 91; apps poll their own state.)
    return true;
}

void SimWifiRadio::loopEntry(void* self) {
    static_cast<SimWifiRadio*>(self)->loopFn();
}

void SimWifiRadio::loopFn() {
    char buf[160];

    while (!loopThread_.shouldStop()) {
        if (doMonitor_) {
            // Generate a minimal fake 802.11 beacon frame so the app gets real
            // non-empty data from monitorRead() without real hardware.
            uint8_t ch;
            { std::lock_guard<std::mutex> g(mu_); ch = monitorChannel_; }

            static const uint8_t kFakeBeacon[32] = {
                0x80, 0x00,                          // FC: beacon
                0x00, 0x00,                          // Duration
                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast
                0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01, // SA: sim AP
                0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01, // BSSID
                0x00, 0x00,                          // Seq ctrl
                0x00, 0x00, 0x00, 0x00,              // Timestamp (low)
                0x00, 0x00, 0x00, 0x00,              // Timestamp (high)
            };
            // Patch channel hint into last byte so callers can identify it.
            uint8_t frame[32];
            std::memcpy(frame, kFakeBeacon, sizeof(frame));
            frame[23] = ch;  // overload seq-ctrl[1] as channel hint
            pushFrame(frame, sizeof(frame));

            Thread::sleepMs(200);  // ~5 fake frames/sec

        } else {
            Thread::sleepMs(10);
        }
    }
}

} // namespace nema
