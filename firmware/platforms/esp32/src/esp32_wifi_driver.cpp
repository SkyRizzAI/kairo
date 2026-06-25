#include "nema/system/capabilities.h"
#include "nema/esp32/esp32_wifi_driver.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/event/async_event_poster.h"
#include "nema/event/event.h"
#include "nema/system/hardware_registry.h"
#include "nema/system/capability_registry.h"
#include "nema/service/service_container.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstdio>
#include <cstring>

namespace nema {

static esp_netif_t* s_sta_netif = nullptr;
static const char* NVS_NS  = "palanu_wifi";
static const int   MAX_SAVED = 4;

// ── saved-network list helpers (NVS) ──
namespace {
struct Saved { char ssid[33]; char pass[65]; bool autojoin; };

static int readSaved(Saved out[MAX_SAVED]) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return 0;
    uint8_t cnt = 0;
    nvs_get_u8(h, "cnt", &cnt);
    if (cnt > MAX_SAVED) cnt = MAX_SAVED;
    int n = 0;
    for (int i = 0; i < cnt; i++) {
        char ks[16], kp[16], ka[16];
        snprintf(ks, sizeof(ks), "s%d", i);
        snprintf(kp, sizeof(kp), "p%d", i);
        snprintf(ka, sizeof(ka), "a%d", i);
        size_t sl = sizeof(out[n].ssid), pl = sizeof(out[n].pass);
        out[n].pass[0] = '\0';
        if (nvs_get_str(h, ks, out[n].ssid, &sl) == ESP_OK && out[n].ssid[0]) {
            nvs_get_str(h, kp, out[n].pass, &pl);
            uint8_t aj = 1;
            nvs_get_u8(h, ka, &aj);
            out[n].autojoin = aj != 0;
            n++;
        }
    }
    nvs_close(h);
    return n;
}

static void writeSaved(const Saved list[], int n) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    if (n > MAX_SAVED) n = MAX_SAVED;
    nvs_set_u8(h, "cnt", (uint8_t)n);
    for (int i = 0; i < n; i++) {
        char ks[16], kp[16], ka[16];
        snprintf(ks, sizeof(ks), "s%d", i);
        snprintf(kp, sizeof(kp), "p%d", i);
        snprintf(ka, sizeof(ka), "a%d", i);
        nvs_set_str(h, ks, list[i].ssid);
        nvs_set_str(h, kp, list[i].pass);
        nvs_set_u8(h, ka, list[i].autojoin ? 1 : 0);
    }
    nvs_commit(h);
    nvs_close(h);
}
} // namespace

void Esp32WifiDriver::onRegister(Runtime& rt) {
    log_    = &rt.log();
    poster_ = &rt.asyncPoster();
    rt.container().registerService(this);
    rt.container().registerAs<IWifiDriver>(this);
    rt.hardware().add({"wifi", DriverKind::Wifi, "ESP32-S3 built-in"});
    rt.capabilities().add(caps::NetWifi);
    caps_ = &rt.capabilities();
}

void Esp32WifiDriver::setState(WifiState s, WifiError e) {
    state_     = s;
    lastError_ = e;
    if (poster_)
        poster_->post({events::WifiStateChanged,
                       {{"state", wifiStateName(s)}, {"err", wifiErrorName(e)}}});
}

void Esp32WifiDriver::start() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        log_->error("Esp32WifiDriver", "esp_wifi_init failed");
        if (caps_) caps_->setState(caps::NetWifi, ResourceState::Fault);
        return;
    }
    esp_wifi_set_mode(WIFI_MODE_STA);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](void* arg, esp_event_base_t, int32_t id, void* data) {
            static_cast<Esp32WifiDriver*>(arg)->onWifiEvent(id, data);
        }, this);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        [](void* arg, esp_event_base_t, int32_t id, void* data) {
            static_cast<Esp32WifiDriver*>(arg)->onWifiEvent(id, data);
        }, this);

    // One-shot timer that retries esp_wifi_connect() with backoff (auto-reconnect).
    esp_timer_create_args_t targs = {};
    targs.callback = [](void* arg) { static_cast<Esp32WifiDriver*>(arg)->reconnectTick(); };
    targs.arg      = this;
    targs.name     = "wifi_reconnect";
    esp_timer_create(&targs, reinterpret_cast<esp_timer_handle_t*>(&reconnectTimer_));

    esp_wifi_start();
    enabled_ = true;
    // Legal channel set (1–11/13); without this scans can miss APs in some regions.
    esp_wifi_set_country_code("01", true);   // "01" = worldwide-safe; board may override
    state_ = WifiState::Idle;
    if (caps_) caps_->setState(caps::NetWifi, ResourceState::Available);
    // Restore the saved IP mode; a static address is pushed onto the netif now so
    // it survives reboots (re-applied here, not just at the moment the user set it).
    loadIpConfig();
    if (staticIp_) applyStaticIp();
    log_->info("Esp32WifiDriver", "started (STA mode)");

    // Non-blocking reconnect to the most-recently-saved network (boot path).
    Saved list[MAX_SAVED];
    if (int n = readSaved(list); n > 0) {
        log_->info("Esp32WifiDriver", std::string("auto-reconnect: ") + list[0].ssid);
        connect(list[0].ssid, list[0].pass);
    }
}

void Esp32WifiDriver::stop() {
    // Guard: stop promiscuous mode if an app left it active (e.g. WiFi Marauder
    // exited without cleanup). esp_wifi_stop() panics if promiscuous is still on.
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    esp_wifi_stop();
    enabled_ = false;
    state_ = WifiState::Disabled;
    if (caps_) caps_->setState(caps::NetWifi, ResourceState::Absent);
    log_->info("Esp32WifiDriver", "stopped");
}

// Wi-Fi on/off (radio power). Off stops the station; On restarts + auto-reconnects.
void Esp32WifiDriver::setEnabled(bool on) {
    if (on == enabled_) return;
    if (on) {
        esp_wifi_start();
        enabled_ = true;
        setState(WifiState::Idle);
        if (caps_) caps_->setState(caps::NetWifi, ResourceState::Available);
        log_->info("Esp32WifiDriver", "radio on");
        autoConnect();
    } else {
        wantConnection_ = false;                // radio off: stop reconnect attempts
        if (reconnectTimer_) esp_timer_stop((esp_timer_handle_t)reconnectTimer_);
        esp_wifi_set_promiscuous(false);        // safe no-op if not active
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        if (connected_) esp_wifi_disconnect();
        esp_wifi_stop();
        enabled_   = false;
        connected_ = false;
        std::strcpy(ip_, "0.0.0.0");
        setState(WifiState::Disabled);
        if (poster_) poster_->post({events::NetworkDisconnected, {{"ssid", ssid_}}});
        log_->info("Esp32WifiDriver", "radio off");
    }
}

bool Esp32WifiDriver::connectSaved(const char* ssid) {
    if (!ssid || !ssid[0]) return false;
    Saved list[MAX_SAVED];
    int n = readSaved(list);
    for (int i = 0; i < n; i++)
        if (std::strcmp(list[i].ssid, ssid) == 0) { connect(ssid, list[i].pass); return true; }
    return false;
}

bool Esp32WifiDriver::connect(const char* ssid, const char* password) {
    strncpy(ssid_, ssid, sizeof(ssid_) - 1);
    strncpy(pass_, password ? password : "", sizeof(pass_) - 1);

    wifi_config_t wcfg = {};
    strncpy((char*)wcfg.sta.ssid,     ssid_, sizeof(wcfg.sta.ssid) - 1);
    strncpy((char*)wcfg.sta.password, pass_, sizeof(wcfg.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    wantConnection_ = true;          // we now want to stay on this network
    retryDelayMs_   = 2000;          // fresh backoff for a fresh attempt
    setState(WifiState::Connecting);
    esp_wifi_connect();
    log_->info("Esp32WifiDriver", std::string("connecting: ") + ssid_);
    saveNetwork(ssid_, pass_);
    return true;
}

void Esp32WifiDriver::disconnect() {
    wantConnection_ = false;         // explicit user intent: stop reconnecting
    if (reconnectTimer_) esp_timer_stop((esp_timer_handle_t)reconnectTimer_);
    esp_wifi_disconnect();
    connected_ = false;
    std::strcpy(ip_, "0.0.0.0");
    setState(WifiState::Idle);
    log_->info("Esp32WifiDriver", "disconnected");
    if (poster_) poster_->post({events::NetworkDisconnected, {{"ssid", ssid_}}});
    ssid_[0] = '\0';
}

// Schedule a backed-off reconnect attempt (ESP-IDF won't retry on its own).
void Esp32WifiDriver::armReconnect() {
    if (!reconnectTimer_) { esp_wifi_connect(); return; }   // fallback: retry now
    esp_timer_stop((esp_timer_handle_t)reconnectTimer_);    // no-op if not armed
    esp_timer_start_once((esp_timer_handle_t)reconnectTimer_, (uint64_t)retryDelayMs_ * 1000);
    retryDelayMs_ = retryDelayMs_ < 16000 ? retryDelayMs_ * 2 : 30000;  // cap at 30 s
}

void Esp32WifiDriver::reconnectTick() {
    if (wantConnection_ && enabled_ && !connected_) esp_wifi_connect();
}

int8_t Esp32WifiDriver::rssi() const {
    if (!connected_) return 0;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) return ap.rssi;
    return 0;
}

// Blocking scan — called from a TaskRunner worker thread.
void Esp32WifiDriver::scan() {
    WifiState prev = state_;
    state_ = WifiState::Scanning;
    scan_.clear();
    wifi_scan_config_t sc = {};   // active scan, all channels
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {   // block=true
        state_ = prev;
        if (poster_) poster_->post({events::WifiScanComplete, {{"count", "0"}}});
        return;
    }
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n > 20) n = 20;
    wifi_ap_record_t recs[20];
    esp_wifi_scan_get_ap_records(&n, recs);
    for (uint16_t i = 0; i < n; i++) {
        WifiNetwork w;
        strncpy(w.ssid, (const char*)recs[i].ssid, sizeof(w.ssid) - 1);
        w.rssi    = recs[i].rssi;
        w.secured = (recs[i].authmode != WIFI_AUTH_OPEN);
        scan_.push_back(w);
    }
    state_ = prev;   // scan doesn't change connection state
    if (poster_)
        poster_->post({events::WifiScanComplete, {{"count", std::to_string(scan_.size())}}});
}

// Live IPv4 info from esp_netif (the actual assigned address), falling back to
// the stored preference for the dhcp flag.
WifiIpConfig Esp32WifiDriver::ipConfig() const {
    WifiIpConfig c = ipcfg_;
    if (s_sta_netif && connected_) {
        esp_netif_ip_info_t info{};
        if (esp_netif_get_ip_info(s_sta_netif, &info) == ESP_OK) {
            esp_ip4addr_ntoa(&info.ip,      c.ip,   sizeof(c.ip));
            esp_ip4addr_ntoa(&info.netmask, c.mask, sizeof(c.mask));
            esp_ip4addr_ntoa(&info.gw,      c.gw,   sizeof(c.gw));
        }
        esp_netif_dns_info_t dns{};
        if (esp_netif_get_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK)
            esp_ip4addr_ntoa(&dns.ip.u_addr.ip4, c.dns, sizeof(c.dns));
    }
    return c;
}

// Push the stored static address onto esp_netif (DHCP off + fixed ip/mask/gw/dns).
void Esp32WifiDriver::applyStaticIp() {
    if (!s_sta_netif) return;
    esp_netif_dhcpc_stop(s_sta_netif);
    esp_netif_ip_info_t info{};
    info.ip.addr      = esp_ip4addr_aton(ipcfg_.ip);
    info.netmask.addr = esp_ip4addr_aton(ipcfg_.mask);
    info.gw.addr      = esp_ip4addr_aton(ipcfg_.gw);
    esp_netif_set_ip_info(s_sta_netif, &info);
    if (ipcfg_.dns[0]) {
        esp_netif_dns_info_t dns{};
        dns.ip.type            = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = esp_ip4addr_aton(ipcfg_.dns);
        esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
    }
}

void Esp32WifiDriver::saveIpConfig() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "ipdhcp", ipcfg_.dhcp ? 1 : 0);
    nvs_set_str(h, "ipaddr", ipcfg_.ip);
    nvs_set_str(h, "ipmask", ipcfg_.mask);
    nvs_set_str(h, "ipgw",   ipcfg_.gw);
    nvs_set_str(h, "ipdns",  ipcfg_.dns);
    nvs_commit(h);
    nvs_close(h);
}

void Esp32WifiDriver::loadIpConfig() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    uint8_t dhcp = 1;
    nvs_get_u8(h, "ipdhcp", &dhcp);
    ipcfg_.dhcp = dhcp != 0;
    size_t l;
    l = sizeof(ipcfg_.ip);   nvs_get_str(h, "ipaddr", ipcfg_.ip,   &l);
    l = sizeof(ipcfg_.mask); nvs_get_str(h, "ipmask", ipcfg_.mask, &l);
    l = sizeof(ipcfg_.gw);   nvs_get_str(h, "ipgw",   ipcfg_.gw,   &l);
    l = sizeof(ipcfg_.dns);  nvs_get_str(h, "ipdns",  ipcfg_.dns,  &l);
    nvs_close(h);
    staticIp_ = !ipcfg_.dhcp;
}

// Switch between DHCP and a static address (applied immediately + persisted).
void Esp32WifiDriver::setIpConfig(const WifiIpConfig& c) {
    ipcfg_    = c;
    staticIp_ = !c.dhcp;
    saveIpConfig();
    if (!s_sta_netif) return;
    if (c.dhcp) esp_netif_dhcpc_start(s_sta_netif);   // re-enable DHCP
    else        applyStaticIp();
    log_->info("Esp32WifiDriver", c.dhcp ? "ip mode: DHCP" : "ip mode: static");
}

// ── sys_evt task — ONLY touch poster_/flags/state. No EventBus, no Logger. ──
void Esp32WifiDriver::onWifiEvent(int32_t event_id, void* data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        connected_   = true;
        retryDelayMs_ = 2000;                 // link is up — reset backoff for next drop
        auto* ev = static_cast<ip_event_got_ip_t*>(data);
        if (ev) esp_ip4addr_ntoa(&ev->ip_info.ip, ip_, sizeof(ip_));
        setState(WifiState::Connected);
        if (poster_) poster_->post({events::NetworkConnected, {{"ssid", ssid_}}});
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        // Static IP: DHCP is off so no GOT_IP fires — connection is "up" at
        // association time, with the fixed address we already pushed onto netif.
        if (staticIp_) {
            connected_ = true;
            std::strncpy(ip_, ipcfg_.ip, sizeof(ip_) - 1);
            setState(WifiState::Connected);
            if (poster_) poster_->post({events::NetworkConnected, {{"ssid", ssid_}}});
        }
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected_ = false;
        std::strcpy(ip_, "0.0.0.0");
        uint8_t reason = 0;
        if (auto* ev = static_cast<wifi_event_sta_disconnected_t*>(data)) reason = ev->reason;
        // A wrong password / failed handshake won't fix itself by retrying — stop and
        // surface the error. Everything else (AP out of range, beacon timeout, the AP
        // rebooting) IS transient: keep retrying so we auto-rejoin when it returns.
        bool credentialIssue = (reason == WIFI_REASON_AUTH_FAIL ||
                                reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                                reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
                                reason == WIFI_REASON_AUTH_EXPIRE);
        if (credentialIssue) {
            wantConnection_ = false;
            setState(WifiState::Failed, WifiError::AuthFailed);
        } else if (wantConnection_ && enabled_) {
            setState(WifiState::Connecting);   // actively retrying (backed off)
            armReconnect();
        } else {
            setState(WifiState::Idle);
        }
        if (poster_) poster_->post({events::NetworkDisconnected, {{"ssid", ssid_}}});
    }
}

// ── saved networks ──
void Esp32WifiDriver::saveNetwork(const char* ssid, const char* password) {
    if (!ssid || !ssid[0]) return;
    Saved list[MAX_SAVED];
    int n = readSaved(list);
    // dedup: if ssid exists, move it to the front (most-recent) and update pass
    int found = -1;
    for (int i = 0; i < n; i++) if (std::strcmp(list[i].ssid, ssid) == 0) { found = i; break; }
    Saved entry{};
    strncpy(entry.ssid, ssid, sizeof(entry.ssid) - 1);
    strncpy(entry.pass, password ? password : "", sizeof(entry.pass) - 1);
    entry.autojoin = (found >= 0) ? list[found].autojoin : true;  // keep existing pref
    Saved out[MAX_SAVED];
    out[0] = entry;
    int m = 1;
    for (int i = 0; i < n && m < MAX_SAVED; i++) {
        if (i == found) continue;
        out[m++] = list[i];
    }
    writeSaved(out, m);
}

void Esp32WifiDriver::forgetNetwork(const char* ssid) {
    Saved list[MAX_SAVED];
    int n = readSaved(list);
    Saved out[MAX_SAVED];
    int m = 0;
    for (int i = 0; i < n; i++)
        if (std::strcmp(list[i].ssid, ssid) != 0) out[m++] = list[i];
    writeSaved(out, m);
}

void Esp32WifiDriver::setAutoJoin(const char* ssid, bool on) {
    Saved list[MAX_SAVED];
    int n = readSaved(list);
    bool changed = false;
    for (int i = 0; i < n; i++)
        if (std::strcmp(list[i].ssid, ssid) == 0) { list[i].autojoin = on; changed = true; }
    if (changed) writeSaved(list, n);
}

size_t Esp32WifiDriver::savedCount() const {
    Saved list[MAX_SAVED];
    return (size_t)readSaved(list);
}

bool Esp32WifiDriver::savedAt(size_t i, WifiProfile& out) const {
    Saved list[MAX_SAVED];
    int n = readSaved(list);
    if ((int)i >= n) return false;
    strncpy(out.ssid, list[i].ssid, sizeof(out.ssid) - 1);
    out.secured  = list[i].pass[0] != '\0';
    out.autoJoin = list[i].autojoin;
    return true;
}

// Blocking — worker thread. Scan, then connect to the strongest saved AP in range.
void Esp32WifiDriver::autoConnect() {
    Saved list[MAX_SAVED];
    int n = readSaved(list);
    if (n == 0) return;
    scan();
    const WifiNetwork* best = nullptr;
    const Saved*       bestSaved = nullptr;
    for (auto& ap : scan_) {
        for (int i = 0; i < n; i++) {
            if (!list[i].autojoin) continue;   // user disabled auto-join for this one
            if (std::strcmp(ap.ssid, list[i].ssid) == 0) {
                if (!best || ap.rssi > best->rssi) { best = &ap; bestSaved = &list[i]; }
            }
        }
    }
    if (bestSaved) { connect(bestSaved->ssid, bestSaved->pass); return; }
    // Not seen in this scan (out of range, or the scan raced esp_wifi_start). Still
    // arm an auto-join to the most-recent auto-join network — connect() sets the
    // config + wantConnection_, so the retry loop rejoins the moment it appears.
    for (int i = 0; i < n; i++)
        if (list[i].autojoin) { connect(list[i].ssid, list[i].pass); return; }
}

} // namespace nema
