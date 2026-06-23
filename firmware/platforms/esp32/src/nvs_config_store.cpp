#include "nema/esp32/nvs_config_store.h"
#include "nema/log/logger.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cstring>

// ── Flash-write proxy ─────────────────────────────────────────────────────────
//
// NVS write operations call spi_flash_disable_interrupts_caches_and_other_cpu(),
// which asserts that the calling task's stack is in internal SRAM (not PSRAM).
// The AppHost task uses a PSRAM stack for large JS/WASM runtimes
// (xTaskCreatePinnedToCoreWithCaps, MALLOC_CAP_SPIRAM), which violates the
// assertion and panics the device.
//
// Fix: all write/erase NVS calls are dispatched to a tiny static worker task
// whose stack is in internal DRAM (declared as DRAM_ATTR static arrays below).
// The caller blocks on s_nvsDone until the worker finishes — same latency
// as a direct call, but executed from an internal-RAM stack.

namespace {

struct NvsJob {
    void (*fn)(void*);  // operation to run
    void* ctx;          // caller-allocated context (stack-valid while caller blocks)
};

// DRAM_ATTR forces these into internal DRAM even when external PSRAM is the
// default heap. The task stack MUST be in internal SRAM for flash operations.
static DRAM_ATTR StackType_t  s_nvsStack[4096 / sizeof(StackType_t)];
static DRAM_ATTR StaticTask_t s_nvsTcb;
static StaticQueue_t   s_nvsQBuf;
static NvsJob          s_nvsQSlot;   // queue depth = 1 (NVS ops are serialised)
static StaticSemaphore_t s_nvsDoneBuf;

static QueueHandle_t     s_nvsQ    = nullptr;
static SemaphoreHandle_t s_nvsDone = nullptr;

static void nvsWorker(void*) {
    NvsJob job;
    for (;;) {
        if (xQueueReceive(s_nvsQ, &job, portMAX_DELAY) == pdTRUE) {
            job.fn(job.ctx);
            xSemaphoreGive(s_nvsDone);
        }
    }
}

static void initNvsWorker() {
    if (s_nvsQ) return;  // already initialised
    s_nvsQ    = xQueueCreateStatic(1, sizeof(NvsJob),
                                   reinterpret_cast<uint8_t*>(&s_nvsQSlot), &s_nvsQBuf);
    s_nvsDone = xSemaphoreCreateBinaryStatic(&s_nvsDoneBuf);
    xTaskCreateStaticPinnedToCore(nvsWorker, "nvs_w",
                                  sizeof(s_nvsStack) / sizeof(s_nvsStack[0]),
                                  nullptr, 5, s_nvsStack, &s_nvsTcb, 0);
}

// Run fn(ctx) on the internal-RAM NVS worker. Blocks until fn returns.
// Lazily creates the worker if start() was not called before the first write.
static void dispatchNvsWrite(void (*fn)(void*), void* ctx) {
    if (!s_nvsQ) {
        initNvsWorker();
    }
    if (!s_nvsQ) {
        // initNvsWorker failed (e.g. called from ISR context) — drop the write
        // rather than crashing from a PSRAM-stack task.
        return;
    }
    NvsJob job{fn, ctx};
    xQueueSend(s_nvsQ, &job, portMAX_DELAY);
    xSemaphoreTake(s_nvsDone, portMAX_DELAY);
}

} // anon namespace

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
    initNvsWorker();
}

// ── Reads (NVS_READONLY does NOT disable cache — safe from any stack) ─────────

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

// ── Writes — dispatched to the internal-RAM NVS worker task ──────────────────

void NvsConfigStore::setString(const char* ns, const char* key, const std::string& val) {
    struct Ctx { const char* ns; const char* key; const char* val; };
    Ctx ctx{ns, key, val.c_str()};
    dispatchNvsWrite([](void* p) {
        auto* x = static_cast<Ctx*>(p);
        nvs_handle_t h;
        if (nvs_open(x->ns, NVS_READWRITE, &h) != ESP_OK) return;
        nvs_set_str(h, x->key, x->val);
        nvs_commit(h);
        nvs_close(h);
    }, &ctx);
}

void NvsConfigStore::setInt(const char* ns, const char* key, int64_t val) {
    struct Ctx { const char* ns; const char* key; int64_t val; };
    Ctx ctx{ns, key, val};
    dispatchNvsWrite([](void* p) {
        auto* x = static_cast<Ctx*>(p);
        nvs_handle_t h;
        if (nvs_open(x->ns, NVS_READWRITE, &h) != ESP_OK) return;
        nvs_set_i64(h, x->key, x->val);
        nvs_commit(h);
        nvs_close(h);
    }, &ctx);
}

bool NvsConfigStore::remove(const char* ns, const char* key) {
    struct Ctx { const char* ns; const char* key; bool result; };
    Ctx ctx{ns, key, false};
    dispatchNvsWrite([](void* p) {
        auto* x = static_cast<Ctx*>(p);
        nvs_handle_t h;
        if (nvs_open(x->ns, NVS_READWRITE, &h) != ESP_OK) return;
        x->result = (nvs_erase_key(h, x->key) == ESP_OK);
        nvs_commit(h);
        nvs_close(h);
    }, &ctx);
    return ctx.result;
}

} // namespace nema
