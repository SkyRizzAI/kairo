#pragma once
#include "nema/hal/display.h"
#include "nema/service.h"
#include <cstdint>
#include <cstddef>

namespace nema {
class Runtime;
}

namespace nema::skyrizzsolana {

// LcdDriver — ILI9341 240×320 TFT over SPI (direct ESP32, FPC1).
//
// Same 1-bit monochrome framebuffer + RGB565-on-flush model as the E32 panel,
// but the backlight (GPIO7) and reset (GPIO14) are DIRECT ESP32 GPIOs here — no
// I/O-expander dependency. Flush is synchronous SPI DMA (< 30 ms at 40 MHz).
class LcdDriver : public IDisplayDriver, public IService {
public:
    void init(Runtime& rt);

    // IDisplayDriver + IDriver
    uint16_t   width()  const override { return width_;  }
    uint16_t   height() const override { return height_; }
    DriverKind kind()   const override { return DriverKind::Display; }
    void drawPixel  (uint16_t x, uint16_t y, bool on)                         override;
    void fillRect   (uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void invertRect (uint16_t x, uint16_t y, uint16_t w, uint16_t h)         override;
    void clear(bool on = false)                                              override;
    void flush()                                                             override;
    void sleep() override { setBacklight(false); }
    void wake()  override { setBacklight(true);  }
    void blitRgb565(const uint8_t* buf,
                    uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h) override;

    void    setRotation(uint8_t r) override;
    uint8_t rotation() const override { return rotation_; }

    void setPalette(uint16_t fg, uint16_t bg) override {
        auto bswap = [](uint16_t c) -> uint16_t { return (uint16_t)((c >> 8) | (c << 8)); };
        setColors(bswap(fg), bswap(bg));
    }
    bool supportsColor() const override { return true; }

    // Backlight is a plain GPIO (no PWM) → brightness maps >0→on, 0→off.
    void    setBrightness(uint8_t level) override;
    uint8_t brightness() const override { return brightness_; }

    void flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) override;

    // IService
    const char* name() const override { return "LcdDriver"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

    void setBacklight(bool on);   // direct GPIO7

    void setColors(uint16_t fg, uint16_t bg) { fgColor_ = fg; bgColor_ = bg; }

private:
    void panelInit();
    void applyMadctl();
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void spiWrite(uint8_t* data, size_t len, bool isData);

    Runtime* rt_       = nullptr;
    // ILI9341 native portrait orientation (matches E32 panel).
    uint16_t width_    = 240;
    uint16_t height_   = 320;
    uint16_t nativeW_  = 240;
    uint16_t nativeH_  = 320;
    uint8_t  rotation_ = 0;
    uint8_t  brightness_ = 255;

    uint8_t* framebuf_ = nullptr;   // 1-bit monochrome: width*height/8 bytes
    void*    spiHandle_ = nullptr;  // spi_device_handle_t (opaque)
    uint16_t fgColor_  = 0xFFFF;
    uint16_t bgColor_  = 0x0000;

    uint8_t* prevBuf_   = nullptr;
    bool     fullFlush_ = true;
};

} // namespace nema::skyrizzsolana
