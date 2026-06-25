#include "nema/esp32/esp32_wifi_radio.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/event/event.h"
#include "nema/event/event_bus.h"
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_heap_caps.h>
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

    // Lifecycle cleanup: when any app exits, stop monitor/inject modes so the
    // radio doesn't stay in promiscuous state. Without this, SystemWifiManager's
    // autoConnect() silently fails (radio in wrong mode), and esp_wifi_stop()
    // panics when settings tries to toggle WiFi off.
    rt.events().subscribe(events::AppHostExited, [this](const Event&) {
        // Tear down anything the app left running on ANY exit path (Back, return,
        // crash, forceQuit). Attacks + the captive portal now live in the app
        // (Plan 91) — on exit they just stop. The kernel only restores the radio
        // modes it owns: drop promiscuous, and if the app left a soft-AP up
        // (e.g. a portal that crashed before wifi_ap_stop), restore STA so
        // SystemWifiManager can reconnect.
        monitorClose();
        wifi_mode_t mode = WIFI_MODE_STA;
        if (esp_wifi_get_mode(&mode) == ESP_OK && mode == WIFI_MODE_AP) apStop();
    });
}

void Esp32WifiRadio::stop() {
    monitorClose();   // disable promiscuous
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
    // Promiscuous MUST be enabled before esp_wifi_set_channel(). Calling
    // set_channel first fails with "STA is connecting/scanning" when the driver
    // hasn't fully transitioned yet, leaving the radio on the AP's channel
    // instead of the requested one → sniff screens always see 0 frames.
    esp_wifi_set_promiscuous_rx_cb(promiscRxCb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    curChannel_ = ch;   // keep inject()'s channel cache in sync
    return true;
}

void Esp32WifiRadio::monitorClose() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    s_radio = nullptr;
    curChannel_ = 0;    // channel state no longer known
}

bool Esp32WifiRadio::inject(uint8_t ch, const uint8_t* frame, size_t len) {
    if (!frame || len == 0 || len > 2500) return false;
    // Only re-tune when the channel actually changes. Re-tuning before every
    // frame (the old behaviour) made rapid bursts like beacon spam fail — the
    // radio spent its time hopping instead of transmitting. ch==0 → keep current.
    if (ch != 0 && ch != curChannel_) {
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        curChannel_ = ch;
    }
    return esp_wifi_80211_tx(WIFI_IF_STA, frame, static_cast<int>(len), false) == ESP_OK;
}

// ── setMac ───────────────────────────────────────────────────────────────────

bool Esp32WifiRadio::setMac(std::string_view mac) {
    if (mac.empty()) return true;
    uint8_t bytes[6];
    if (!parseBssid(mac, bytes)) return false;
    return esp_wifi_set_mac(WIFI_IF_STA, bytes) == ESP_OK;
}

// ── Soft AP (generic primitive — Plan 91) ──────────────────────────────────
// EXACT sequence the (working) evil portal used: clear promiscuous, create the
// AP netif+DHCP once, then stop -> set_mode(AP) -> set_config -> start. Merely
// set_mode/set_config on a running STA does NOT start the beacon — the start()
// after the reconfigure is what makes the SSID appear. Do not reorder this.
bool Esp32WifiRadio::apStart(std::string_view ssid, uint8_t channel, bool open) {
    (void)open;  // only open auth supported for now (captive portals)
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    static esp_netif_t* s_ap_netif = nullptr;
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (!s_ap_netif) s_ap_netif = esp_netif_create_default_wifi_ap();
    }

    esp_wifi_stop();
    esp_err_t mode_rc = esp_wifi_set_mode(WIFI_MODE_AP);

    std::string s(ssid);
    wifi_config_t ap_cfg = {};
    std::strncpy(reinterpret_cast<char*>(ap_cfg.ap.ssid), s.c_str(),
                 sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len        = static_cast<uint8_t>(s.size() < 32 ? s.size() : 32);
    ap_cfg.ap.channel         = channel ? channel : 1;
    ap_cfg.ap.authmode        = WIFI_AUTH_OPEN;
    ap_cfg.ap.ssid_hidden     = 0;
    ap_cfg.ap.max_connection  = 4;
    ap_cfg.ap.beacon_interval = 100;
    esp_err_t cfg_rc   = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_err_t start_rc = esp_wifi_start();

    // Make the AP's DHCP server hand out 192.168.4.1 as the DNS server. Without
    // this, clients keep their cellular/cached DNS, so a captive-portal app's
    // DNS catch-all is never queried and the OS never shows the login page —
    // the client just times out and leaves. This is what makes captive work.
    if (s_ap_netif) {
        esp_netif_dhcps_stop(s_ap_netif);
        esp_netif_dns_info_t dns = {};
        dns.ip.type            = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = inet_addr("192.168.4.1");
        esp_netif_set_dns_info(s_ap_netif, ESP_NETIF_DNS_MAIN, &dns);
        uint8_t offer_dns = 1;   // DHCP offers the DNS option to clients
        esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET,
                               ESP_NETIF_DOMAIN_NAME_SERVER,
                               &offer_dns, sizeof(offer_dns));
        esp_netif_dhcps_start(s_ap_netif);
    }

    if (rt_) rt_->log().info("WifiAP", "started",
        {{"ssid", s}, {"netif", s_ap_netif ? "ok" : "FAILED"},
         {"set_mode", esp_err_to_name(mode_rc)},
         {"set_cfg", esp_err_to_name(cfg_rc)},
         {"start", esp_err_to_name(start_rc)}});
    return start_rc == ESP_OK;
}

bool Esp32WifiRadio::apStop() {
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();   // STA reconnect is driven by SystemWifiManager on lease release
    curChannel_ = 0;    // AP cycle changed the radio — force inject() to re-tune
    return true;
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
