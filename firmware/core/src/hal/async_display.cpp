#include "nema/hal/async_display.h"
#include "nema/log/logger.h"
#include <cstring>
#include <cstdlib>
#include <utility>

#ifdef ESP_PLATFORM
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
  #include <freertos/semphr.h>
  #include <esp_heap_caps.h>
#endif

namespace nema {

// Buffer allocation: PSRAM on ESP32 (these are ~46KB each), plain heap on host.
static uint8_t* allocBuf(size_t n) {
#ifdef ESP_PLATFORM
    if (auto* p = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM)) return p;
    return (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_8BIT);  // fallback: internal
#else
    return (uint8_t*)std::malloc(n);
#endif
}

void AsyncDisplayDriver::init(IDisplayDriver& inner, Logger& log) {
    inner_ = &inner;
    log_   = &log;
    w_     = inner.width();
    h_     = inner.height();
    size_  = (size_t)w_ * h_;

    draw_buf_  = allocBuf(size_);
    ready_buf_ = allocBuf(size_);
    show_buf_  = allocBuf(size_);
    if (draw_buf_)  std::memset(draw_buf_,  0, size_);
    if (ready_buf_) std::memset(ready_buf_, 0, size_);
    if (show_buf_)  std::memset(show_buf_,  0, size_);
}

// ── Draw ops — main task, write to draw_buf_, never block ─────────────────

void AsyncDisplayDriver::drawPixel(uint16_t x, uint16_t y, bool on) {
    if (x >= w_ || y >= h_ || !draw_buf_) return;
    draw_buf_[(size_t)y * w_ + x] = on ? 1 : 0;
}

void AsyncDisplayDriver::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    if (!draw_buf_) return;
    for (uint16_t row = y; row < y + h && row < h_; row++) {
        uint16_t x1 = x < w_ ? x : w_;
        uint16_t x2 = x + w < w_ ? x + w : w_;
        if (x1 < x2) std::memset(draw_buf_ + (size_t)row * w_ + x1, on ? 1 : 0, x2 - x1);
    }
}

void AsyncDisplayDriver::clear(bool on) {
    if (draw_buf_) std::memset(draw_buf_, on ? 1 : 0, size_);
}

void AsyncDisplayDriver::invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!draw_buf_) return;
    for (uint16_t row = y; row < y + h && row < h_; row++)
        for (uint16_t col = x; col < x + w && col < w_; col++)
            draw_buf_[(size_t)row * w_ + col] ^= 1;
}

#ifdef ESP_PLATFORM
// ───────────────────────── ESP32: FreeRTOS task ──────────────────────────

void AsyncDisplayDriver::start() {
    signal_sem_ = xSemaphoreCreateBinary();
    running_    = true;

    TaskHandle_t h = nullptr;
    xTaskCreatePinnedToCore(
        taskEntry, "async_display",
        8192, this,
        4,   // priority below the Arduino loopTask if it shared a core
        &h,
        0    // core 0 (PRO_CPU). Arduino loopTask runs on core 1 (APP_CPU),
             // so display refresh runs truly parallel — never steals the main
             // loop's core. WiFi also lives on core 0 but the refresh is mostly
             // I/O wait (GxEPD2 yields via delay), so it won't starve WiFi.
    );
    task_ = h;
    if (log_) log_->info("AsyncDisplay", "display task started",
                         {{"w", std::to_string(w_)}, {"h", std::to_string(h_)}});
}

void AsyncDisplayDriver::stop() {
    running_ = false;
    if (signal_sem_) xSemaphoreGive((SemaphoreHandle_t)signal_sem_);  // unblock task
    if (task_) { vTaskDelete((TaskHandle_t)task_); task_ = nullptr; }
    if (signal_sem_) { vSemaphoreDelete((SemaphoreHandle_t)signal_sem_); signal_sem_ = nullptr; }
}

void AsyncDisplayDriver::flush() {
    // Stage the latest frame and wake the task. NEVER drops: if the task is
    // busy, dirty_ stays set and the task re-renders this frame when it finishes.
    {
        std::lock_guard<std::mutex> lk(mtx_);
        std::swap(draw_buf_, ready_buf_);  // ready_buf_ = newest complete frame
        dirty_ = true;
        // draw_buf_ now holds an old frame — harmless: the runtime calls clear()
        // before every draw(), so it is fully overwritten next frame.
    }
    xSemaphoreGive((SemaphoreHandle_t)signal_sem_);
}

void AsyncDisplayDriver::taskEntry(void* arg) {
    static_cast<AsyncDisplayDriver*>(arg)->serviceLoop();
}

void AsyncDisplayDriver::serviceLoop() {
    while (running_) {
        xSemaphoreTake((SemaphoreHandle_t)signal_sem_, portMAX_DELAY);
        // Drain: render the latest staged frame, and if a newer one arrives
        // during the (slow) refresh, render that too. Final state always shown.
        while (running_) {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                if (!dirty_) break;
                std::swap(ready_buf_, show_buf_);  // take latest into show_buf_
                dirty_ = false;
            }
            inner_->flushBuffer(show_buf_, w_, h_);  // slow SPI work, isolated
        }
    }
    vTaskDelete(nullptr);
}

#else
// ───────────────────────── Host: synchronous passthrough ─────────────────

void AsyncDisplayDriver::start() { running_ = true; }
void AsyncDisplayDriver::stop()  { running_ = false; }

void AsyncDisplayDriver::flush() {
    if (inner_) inner_->flushBuffer(draw_buf_, w_, h_);
}

void AsyncDisplayDriver::taskEntry(void*) {}
void AsyncDisplayDriver::serviceLoop() {}

#endif

} // namespace nema
