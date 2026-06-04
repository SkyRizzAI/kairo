// Nema Thread — ESP32 implementation via FreeRTOS task.
#include "kairo/nema/thread.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace kairo::nema {

void Thread::trampoline(void* self_) {
    auto* self = static_cast<Thread*>(self_);
    self->entry_(self->arg_);
    self->running_.store(false);
    xSemaphoreGive((SemaphoreHandle_t)self->done_);  // signal join()
    vTaskDelete(nullptr);                            // task self-deletes
}

void Thread::start(const ThreadConfig& cfg, Entry entry, void* arg) {
    entry_ = entry;
    arg_   = arg;
    stop_.store(false);
    running_.store(true);
    done_ = xSemaphoreCreateBinary();

    TaskHandle_t h = nullptr;
    // ESP-IDF xTaskCreate stack depth is in BYTES (unlike vanilla FreeRTOS = words).
    if (cfg.core < 0) {
        xTaskCreate(trampoline, cfg.name, cfg.stackBytes, this, cfg.priority, &h);
    } else {
        xTaskCreatePinnedToCore(trampoline, cfg.name, cfg.stackBytes, this,
                                cfg.priority, &h, cfg.core);
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
    vSemaphoreDelete((SemaphoreHandle_t)done_);
    done_ = nullptr;
    os_   = nullptr;
}

Thread::~Thread() {
    requestStop();
    join();
}

} // namespace kairo::nema
