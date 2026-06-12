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
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>

namespace nema {

static esp_netif_t* s_sta_netif = nullptr;
static const char* NVS_NS = "palanu_wifi";

void Esp32WifiDriver::onRegister(Runtime& rt) {
    log_    = &rt.log();
    poster_ = &rt.asyncPoster();
    rt.container().registerService(this);
    rt.container().registerAs<IWifiDriver>(this);
    rt.hardware().add({"wifi", DriverKind::Wifi, "ESP32-S3 built-in"});
    rt.capabilities().add("wifi");
    rt.capabilities().add("networking");
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
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](void* arg, esp_event_base_t, int32_t id, void* data) {
            static_cast<Esp32WifiDriver*>(arg)->onWifiEvent(id, data);
        }, this);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        [](void* arg, esp_event_base_t, int32_t id, void* data) {
            static_cast<Esp32WifiDriver*>(arg)->onWifiEvent(id, data);
        }, this);

    esp_wifi_start();
    log_->info("Esp32WifiDriver", "started (STA mode)");

    loadCredentials();   // auto-reconnect if a network was saved
}

void Esp32WifiDriver::stop() {
    esp_wifi_stop();
    log_->info("Esp32WifiDriver", "stopped");
}

bool Esp32WifiDriver::connect(const char* ssid, const char* password) {
    strncpy(ssid_, ssid, sizeof(ssid_) - 1);
    strncpy(pass_, password ? password : "", sizeof(pass_) - 1);

    wifi_config_t wcfg = {};
    strncpy((char*)wcfg.sta.ssid,     ssid_, sizeof(wcfg.sta.ssid) - 1);
    strncpy((char*)wcfg.sta.password, pass_, sizeof(wcfg.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    esp_wifi_connect();
    connecting_ = true;
    log_->info("Esp32WifiDriver", std::string("connecting: ") + ssid_);
    saveCredentials();
    return true;
}

void Esp32WifiDriver::disconnect() {
    esp_wifi_disconnect();
    connected_  = false;
    connecting_ = false;
    std::strcpy(ip_, "0.0.0.0");
    log_->info("Esp32WifiDriver", "disconnected");
    if (poster_) poster_->post({events::NetworkDisconnected, {{"ssid", ssid_}}});
    ssid_[0] = '\0';
}

// Blocking scan — called from a TaskRunner worker thread.
void Esp32WifiDriver::scan() {
    scan_.clear();
    wifi_scan_config_t sc = {};   // active scan, all channels
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {   // block=true
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
    if (poster_)
        poster_->post({events::WifiScanComplete, {{"count", std::to_string(scan_.size())}}});
}

WifiIpConfig Esp32WifiDriver::ipConfig() const { return ipcfg_; }

void Esp32WifiDriver::setIpConfig(const WifiIpConfig& c) {
    ipcfg_    = c;
    staticIp_ = !c.dhcp;
    // Applying static IP to esp_netif is left for a follow-up; DHCP is default.
}

// ── sys_evt task — ONLY touch poster_/flags. No EventBus, no Logger. ──
void Esp32WifiDriver::onWifiEvent(int32_t event_id, void* data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        connected_  = true;
        connecting_ = false;
        auto* ev = static_cast<ip_event_got_ip_t*>(data);
        if (ev) esp_ip4addr_ntoa(&ev->ip_info.ip, ip_, sizeof(ip_));
        if (poster_) poster_->post({events::NetworkConnected, {{"ssid", ssid_}}});
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected_  = false;
        connecting_ = false;
        std::strcpy(ip_, "0.0.0.0");
        if (poster_) poster_->post({events::NetworkDisconnected, {{"ssid", ssid_}}});
    }
}

void Esp32WifiDriver::saveCredentials() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "ssid", ssid_);
    nvs_set_str(h, "pass", pass_);
    nvs_commit(h);
    nvs_close(h);
}

void Esp32WifiDriver::loadCredentials() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t sl = sizeof(ssid_), pl = sizeof(pass_);
    bool ok = (nvs_get_str(h, "ssid", ssid_, &sl) == ESP_OK) &&
              (nvs_get_str(h, "pass", pass_, &pl) == ESP_OK);
    nvs_close(h);
    if (ok && ssid_[0]) {
        log_->info("Esp32WifiDriver", std::string("auto-reconnect: ") + ssid_);
        connect(ssid_, pass_);
    }
}

} // namespace nema
