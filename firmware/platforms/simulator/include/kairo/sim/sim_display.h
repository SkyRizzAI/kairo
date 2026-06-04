#pragma once
#include "kairo/hal/display.h"
#include "kairo/service.h"
#include <cstdint>

namespace kairo {

class Logger;
class TelemetryBridge;

// 1-bit monochrome virtual display with configurable resolution.
// Dimensions read from KAIRO_SIM_W / KAIRO_SIM_H env vars (default 264×176).
// Buffer: 1 byte per pixel (0=background/white, 1=foreground/ink), heap-allocated.
// flush() emits {"type":"frame"} JSON line via TelemetryBridge.
class SimDisplay : public IDisplayDriver, public IService {
public:
    ~SimDisplay() override;

    void init(Logger& log, TelemetryBridge& bridge);

    // IDriver
    const char* name() const override { return "SimDisplay"; }
    DriverKind  kind() const override { return DriverKind::Display; }

    // IDisplayDriver
    uint16_t width()  const override { return w_; }
    uint16_t height() const override { return h_; }
    void drawPixel(uint16_t x, uint16_t y, bool on) override;
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void clear(bool on = false) override;
    void flush() override;
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    void sleep() override;
    void wake()  override;

    // IService
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

private:
    Logger*          log_    = nullptr;
    TelemetryBridge* bridge_ = nullptr;
    uint16_t         w_      = 264;
    uint16_t         h_      = 176;
    uint8_t*         buf_    = nullptr;  // w_*h_ bytes, 0=bg 1=fg (heap)
};

} // namespace kairo
