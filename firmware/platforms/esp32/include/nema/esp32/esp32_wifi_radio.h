#pragma once
#include "nema/hal/radio_wifi.h"
#include "nema/service.h"
#include "nema/thread.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

namespace nema {

// Esp32WifiRadio — ESP32 hardware implementation of IRadioWifi.
//
// Generic radio PRIMITIVES only — never app-specific attack logic. Apps build
// their own deauth/beacon/probe/karma frames and inject() them in their own
// loop (Plan 91). This driver exposes:
//   scan()        — esp_wifi_scan_start (blocking) → real BSSIDs/RSSIs.
//   monitorOpen() — enable promiscuous; RX callback pushes raw frames into the
//                   base-class ring (pushFrame); app drains via monitorRead().
//   inject()      — esp_wifi_80211_tx (raw frame TX).
//   setMac()      — radio MAC.
//   apStart/Stop  — generic soft AP (apps build captive portals from AP +
//                   INetSockets; the kernel has no "evil portal" — Plan 91).
class Esp32WifiRadio : public IRadioWifi, public IService {
public:
    void init(Runtime& rt);

    // ── IRadioWifi (generic primitives) ───────────────────────────────────────
    std::vector<RadioScanResult> scan() override;
    bool monitorOpen(uint8_t ch)  override;
    void monitorClose()           override;
    bool inject(uint8_t ch, const uint8_t* frame, size_t len) override;
    bool setMac(std::string_view mac) override;
    bool apStart(std::string_view ssid, uint8_t channel, bool open) override;
    bool apStop() override;
    int  staStatus(char* out, uint32_t max) override;
    int  arpScan(char* out, uint32_t max) override;
    int  tcpProbe(std::string_view host, uint16_t port,
                  uint32_t timeoutMs) override;

    // ── IService ──────────────────────────────────────────────────────────────
    const char*  name() const override { return "Esp32WifiRadio"; }
    DriverKind   kind() const override { return DriverKind::Wifi; }
    void start() override {}
    void stop()  override;

private:
    static bool parseBssid(std::string_view s, uint8_t out[6]);

    Runtime*   rt_ = nullptr;
    std::mutex mu_;
    // Last channel we tuned to (0 = unknown). inject() skips a redundant
    // esp_wifi_set_channel when the channel hasn't changed — re-tuning on every
    // frame starves rapid back-to-back TX (e.g. beacon spam: 8 frames/round).
    uint8_t    curChannel_ = 0;
};

} // namespace nema
