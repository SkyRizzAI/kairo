// Nema Thread — ESP32 implementation via FreeRTOS task.
#include "nema/thread.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/idf_additions.h>   // xTaskCreate…WithCaps / vTaskDeleteWithCaps
#include <esp_heap_caps.h>

namespace nema {

// Threads asking for a large stack (the JS/QuickJS app — hundreds of KB) get their
// stack in PSRAM instead of scarce internal RAM. The board enables
// CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY. Smaller threads keep the normal
// internal dynamic stack.
static constexpr uint32_t kPsramStackThreshold = 96 * 1024;

void Thread::trampoline(void* self_) {
    auto* self = static_cast<Thread*>(self_);
    self->entry_(self->arg_);
    self->running_.store(false);
    xSemaphoreGive((SemaphoreHandle_t)self->done_);  // signal join()
    if (self->capsTask_) {
        // PSRAM stack (WithCaps): join() frees it via vTaskDeleteWithCaps from
        // another core's context. Park here so our own stack isn't freed under us.
        vTaskSuspend(nullptr);
    } else {
        vTaskDelete(nullptr);                        // internal stack: idle frees it
    }
}

void Thread::start(const ThreadConfig& cfg, Entry entry, void* arg) {
    entry_ = entry;
    arg_   = arg;
    stop_.store(false);
    running_.store(true);
    done_ = xSemaphoreCreateBinary();

    TaskHandle_t h = nullptr;
    BaseType_t rc;
    // ESP-IDF xTaskCreate stack depth is in BYTES (unlike vanilla FreeRTOS = words).
    // PSRAM stack when the caller asks for it OR the stack is huge (JS/QuickJS) —
    // keeps scarce internal RAM free for DMA/ISR allocations.
    if (cfg.psram || cfg.stackBytes >= kPsramStackThreshold) {
        capsTask_ = true;
        BaseType_t core = cfg.core < 0 ? tskNO_AFFINITY : cfg.core;
        rc = xTaskCreatePinnedToCoreWithCaps(trampoline, cfg.name, cfg.stackBytes,
                                             this, cfg.priority, &h, core,
                                             MALLOC_CAP_SPIRAM);
    } else if (cfg.core < 0) {
        rc = xTaskCreate(trampoline, cfg.name, cfg.stackBytes, this, cfg.priority, &h);
    } else {
        rc = xTaskCreatePinnedToCore(trampoline, cfg.name, cfg.stackBytes, this,
                                     cfg.priority, &h, cfg.core);
    }
    if (rc != pdPASS || h == nullptr) {
        // Could not allocate the task (e.g. not enough RAM for the stack). Fail
        // loudly instead of leaving a half-started Thread the owner waits on
        // forever — the caller sees running()==false and recovers. Delete the
        // completion semaphore here: the caller bails out without join(), so
        // nothing else will free it — leaking it on every failed start() was
        // draining internal RAM ~260 B at a time.
        capsTask_ = false;
        running_.store(false);
        vSemaphoreDelete((SemaphoreHandle_t)done_);
        done_ = nullptr;
        os_ = nullptr;
        return;
    }
    os_ = h;
}

void Thread::sleepMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void Thread::requestStop() { stop_.store(true); }

void Thread::join() {
    if (!done_) return;
    xSemaphoreTake((SemaphoreHandle_t)done_, portMAX_DELAY);  // wait entry return
    if (capsTask_ && os_) {
        // The task signalled done then suspended itself; delete it + free its
        // PSRAM stack from this context (vTaskDeleteWithCaps must not self-run).
        vTaskDelay(pdMS_TO_TICKS(2));   // let it actually reach vTaskSuspend
        vTaskDeleteWithCaps((TaskHandle_t)os_);
    }
    vSemaphoreDelete((SemaphoreHandle_t)done_);
    done_ = nullptr;
    os_   = nullptr;
}

Thread::~Thread() {
    requestStop();
    join();
}

} // namespace nema
