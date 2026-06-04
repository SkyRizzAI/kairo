#pragma once
#include "kairo/hal/display.h"
#include "kairo/service.h"
#include <cstdint>
#include <cstddef>

namespace kairo {
class Runtime;
}

namespace kairo::skyrizze32 {

class Xl9535;

// LcdDriver — TFT LCD display via SPI FPC1.
//
// Panel controller is TBD (probe from ID register at start). Likely ST7789 /
// ILI9341 / GC9A01 depending on the fitted panel. Controller is abstracted
// behind `panelInit()` which is swapped at bring-up.
//
// 1-bit monochrome framebuffer (same as e-ink dev board). Flush is synchronous
// SPI DMA — much faster than e-ink (< 20ms). Canvas layer unchanged.
class LcdDriver : public IDisplayDriver, public IService {
public:
    void init(Runtime& rt, Xl9535& expander);

    // IDisplayDriver
    uint16_t width()  const override { return width_;  }
    uint16_t height() const override { return height_; }
    void drawPixel(uint16_t x, uint16_t y, bool on)                  override;
    void fillRect (uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void flush()                                                       override;

    // IService
    const char* name() const override { return "LcdDriver"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

    void setBacklight(bool on);  // delegates to expander_

private:
    void panelInit();            // controller-specific init sequence
    void spiWrite(uint8_t* data, size_t len, bool isData);

    Runtime* rt_       = nullptr;
    Xl9535*  expander_ = nullptr;
    uint16_t width_    = 320;   // default; overridden by config or probe
    uint16_t height_   = 240;

    uint8_t* framebuf_ = nullptr;   // 1-bit monochrome: width*height/8 bytes
    void*    spiHandle_ = nullptr;  // spi_device_handle_t (opaque to avoid esp-idf in header)
};

} // namespace kairo::skyrizze32
