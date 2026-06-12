#pragma once
#include "nema/hal/display.h"
#include "nema/service.h"
#include <mutex>
#include <cstdint>
#include <cstddef>

namespace nema {

class Logger;

// AsyncDisplayDriver — makes ANY IDisplayDriver non-blocking, generically.
//
// Slow panels (e-ink: 0.5-1.5s per refresh) must not block the main loop.
// This wrapper owns a triple buffer and (on ESP32) a dedicated FreeRTOS task:
//
//   main task   draw → draw_buf_           (always fast)
//   flush()     swap draw_buf_↔ready_buf_, mark dirty, signal task; returns now
//   task        render show_buf_ to glass  (slow, isolated, on core 1)
//
// LATEST-WINS coalescing: if flush() is called while the task is mid-refresh,
// the newest frame is staged in ready_buf_. When the task finishes it checks
// the dirty flag and renders the staged frame too. Intermediate frames are
// skipped (fine on e-ink) but the FINAL state is ALWAYS shown — no stale screen.
//
// The wrapped panel only implements flushBuffer() and any panel-specific
// optimisation (dirty-rect, partial refresh). It needs ZERO threading knowledge.
//
// Host/simulator (no ESP_PLATFORM): flush() is a synchronous passthrough.
class AsyncDisplayDriver : public IDisplayDriver, public IService {
public:
    void init(IDisplayDriver& inner, Logger& log);

    // IDriver
    const char* name() const override { return "AsyncDisplay"; }
    DriverKind  kind() const override { return DriverKind::Display; }

    // IDisplayDriver — draw ops target draw_buf_ (never block)
    uint16_t width()  const override { return w_; }
    uint16_t height() const override { return h_; }
    void drawPixel(uint16_t x, uint16_t y, bool on) override;
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void clear(bool on = false) override;
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    void flush() override;   // non-blocking (ESP32) / synchronous (host)

    // IService — task lifecycle
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

private:
    IDisplayDriver* inner_ = nullptr;
    Logger*         log_   = nullptr;

    // Triple buffer (latest-wins handoff):
    uint8_t* draw_buf_  = nullptr;  // main task draws here
    uint8_t* ready_buf_ = nullptr;  // staged latest complete frame (handoff slot)
    uint8_t* show_buf_  = nullptr;  // display task renders from here
    uint16_t w_ = 0, h_ = 0;
    size_t   size_ = 0;

    std::mutex mtx_;            // guards ready_buf_ swap + dirty_ (ESP32 path)
    [[maybe_unused]] bool dirty_ = false;  // newer frame staged (ESP32 path only)

    [[maybe_unused]] void* task_       = nullptr;  // TaskHandle_t
    [[maybe_unused]] void* signal_sem_ = nullptr;  // SemaphoreHandle_t — wake task
    bool  running_ = false;

    static void taskEntry(void* arg);
    void        serviceLoop();
};

} // namespace nema
