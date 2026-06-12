#include "nema/esp32/nvs_config_store.h"
#include "nema/log/logger.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>

namespace nema {

void NvsConfigStore::start() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        if (log_) log_->warn("NvsConfig", "nvs_flash_init failed, config will not persist");
    } else {
        if (log_) log_->info("NvsConfig", "started");
    }
}

bool NvsConfigStore::getString(const char* ns, const char* key, std::string& out) const {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return false;

    size_t len = 0;
    esp_err_t err = nvs_get_str(h, key, nullptr, &len);
    if (err != ESP_OK) { nvs_close(h); return false; }

    out.resize(len);
    err = nvs_get_str(h, key, &out[0], &len);
    nvs_close(h);

    if (err != ESP_OK) return false;
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return true;
}

bool NvsConfigStore::getInt(const char* ns, const char* key, int64_t& out) const {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t err = nvs_get_i64(h, key, &out);
    nvs_close(h);
    return err == ESP_OK;
}

void NvsConfigStore::setString(const char* ns, const char* key, const std::string& val) {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, key, val.c_str());
    nvs_commit(h);
    nvs_close(h);
}

void NvsConfigStore::setInt(const char* ns, const char* key, int64_t val) {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i64(h, key, val);
    nvs_commit(h);
    nvs_close(h);
}

bool NvsConfigStore::remove(const char* ns, const char* key) {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

} // namespace nema
