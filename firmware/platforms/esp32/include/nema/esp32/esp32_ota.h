#pragma once
#include "nema/hal/ota.h"
#include "esp_ota_ops.h"

namespace nema {

// Esp32OtaUpdater — IOtaUpdater on top of ESP-IDF esp_ota_* (Plan 39). Streams an
// incoming image into the inactive app slot (A/B partitions), verifies it on
// commit, and sets it as the next boot. Transport-agnostic: fed by RemoteService
// (PLP push) or a future WiFi pull — it only sees begin/write/commit.
class Esp32OtaUpdater : public IOtaUpdater {
public:
    bool        supported() const override {
        return esp_ota_get_next_update_partition(nullptr) != nullptr;
    }
    bool        begin(uint32_t totalSize) override;
    bool        write(const uint8_t* data, size_t len) override;
    bool        commit() override;
    void        abort() override;
    uint32_t    written() const override { return written_; }
    const char* runningSlot() const override;
    void        confirmBoot() override;   // cancel pending rollback once booted OK

private:
    esp_ota_handle_t       handle_  = 0;
    const esp_partition_t* target_  = nullptr;
    uint32_t               written_ = 0;
    bool                   active_  = false;
};

} // namespace nema
