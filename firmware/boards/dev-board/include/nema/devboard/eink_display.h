#pragma once
#include "nema/hal/display.h"
#include "nema/service.h"
#include <cstdint>

namespace nema {

class Logger;

// EinkDisplay — GDEY027T91 2.7" 264×176 panel via GxEPD2.
//
// This is a PLAIN, SYNCHRONOUS panel. It knows nothing about threading.
// Non-blocking behaviour is provided generically by AsyncDisplayDriver, which
// wraps this panel and calls flushBuffer() from a dedicated display task.
//
// Panel-specific cleverness that DOES belong here:
//   - dirty-rect diff (only redraw changed region)
//   - partial vs full refresh (full every N frames to clear e-ink ghosting)
//   - GxEPD2 paged SPI transfer
//
// flushBuffer() blocking is fine — it runs in AsyncDisplayDriver's task.
class EinkDisplay : public IDisplayDriver, public IService {
public:
    static constexpr uint16_t W = 264;
    static constexpr uint16_t H = 176;

    void init(Logger& log);

    // IDriver
    const char* name() const override { return "EinkDisplay"; }
    DriverKind  kind() const override { return DriverKind::Display; }

    // IDisplayDriver — standalone path (buf_); normally unused since the panel
    // is driven via flushBuffer() by AsyncDisplayDriver.
    uint16_t width()  const override { return W; }
    uint16_t height() const override { return H; }
    void drawPixel(uint16_t x, uint16_t y, bool on) override;
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void clear(bool on = false) override;
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    void flush() override;  // standalone: flushBuffer(buf_)

    // Primary hook: push a raw buffer to the glass. Called by AsyncDisplayDriver.
    void flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) override;

    // IService — SPI + GxEPD2 init. No task is created here anymore.
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

private:
    static constexpr uint8_t FULL_REFRESH_EVERY = 30;

    Logger*  log_ = nullptr;
    uint8_t* buf_      = nullptr;  // standalone shadow (PSRAM)
    uint8_t* prev_buf_ = nullptr;  // last-sent state for dirty-rect (PSRAM)
    uint8_t  partial_count_ = 0;
};

} // namespace nema
