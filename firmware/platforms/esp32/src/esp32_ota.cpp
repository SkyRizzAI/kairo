#include "nema/esp32/esp32_ota.h"
#include "sdkconfig.h"

namespace nema {

bool Esp32OtaUpdater::begin(uint32_t totalSize) {
    target_ = esp_ota_get_next_update_partition(nullptr);   // the inactive slot
    if (!target_) return false;
    written_ = 0;
    esp_err_t e = esp_ota_begin(target_, totalSize ? totalSize : OTA_SIZE_UNKNOWN, &handle_);
    active_ = (e == ESP_OK);
    return active_;
}

bool Esp32OtaUpdater::write(const uint8_t* data, size_t len) {
    if (!active_) return false;
    if (esp_ota_write(handle_, data, len) != ESP_OK) { abort(); return false; }
    written_ += (uint32_t)len;
    return true;
}

bool Esp32OtaUpdater::commit() {
    if (!active_) return false;
    active_ = false;
    if (esp_ota_end(handle_) != ESP_OK) return false;          // validates the image
    return esp_ota_set_boot_partition(target_) == ESP_OK;
}

void Esp32OtaUpdater::abort() {
    if (active_) { esp_ota_abort(handle_); active_ = false; }
}

const char* Esp32OtaUpdater::runningSlot() const {
    const esp_partition_t* p = esp_ota_get_running_partition();
    return p ? p->label : "?";
}

void Esp32OtaUpdater::confirmBoot() {
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    // If this image is on probation (just OTA'd), mark it valid so the bootloader
    // stops the rollback watch. Reaching here means the firmware booted far enough
    // to register its services.
    const esp_partition_t* p = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (p && esp_ota_get_state_partition(p, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY)
        esp_ota_mark_app_valid_cancel_rollback();
#endif
}

} // namespace nema
