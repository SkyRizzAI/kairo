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

    // IDisplayDriver + IDriver
    uint16_t   width()  const override { return width_;  }
    uint16_t   height() const override { return height_; }
    DriverKind kind()   const override { return DriverKind::Display; }
    void drawPixel  (uint16_t x, uint16_t y, bool on)                         override;
    void fillRect   (uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void invertRect (uint16_t x, uint16_t y, uint16_t w, uint16_t h)         override;
    void clear(bool on = false)                                              override;
    void flush()                                                             override;
    // Direct RGB565 blit — bypasses 1-bit framebuf for camera/color content.
    // Handles panel inversion (inverts each pixel before SPI write).
    void blitRgb565(const uint8_t* buf,
                    uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h) override;

    // Fast full-screen blit of a 1-byte-per-pixel mono buffer (1=ink, 0=bg)
    // straight to the panel in one pass — skips the per-pixel drawPixel loop
    // AND the 1-bit framebuffer. Used by AppHost for fullscreen apps. Assumes
    // w==width_, h==height_.
    void flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) override;

    // IService
    const char* name() const override { return "LcdDriver"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t) override {}

    void setBacklight(bool on);  // delegates to expander_

    // RGB565 palette — override before start() if desired.
    // Defaults: white ink on black background.
    void setColors(uint16_t fg, uint16_t bg) { fgColor_ = fg; bgColor_ = bg; }

private:
    void panelInit();
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void spiWrite(uint8_t* data, size_t len, bool isData);

    Runtime* rt_       = nullptr;
    Xl9535*  expander_ = nullptr;
    // ILI9341 native portrait orientation on this board (matches Rust reference).
    uint16_t width_    = 240;
    uint16_t height_   = 320;

    uint8_t* framebuf_ = nullptr;   // 1-bit monochrome: width*height/8 bytes
    void*    spiHandle_ = nullptr;  // spi_device_handle_t (opaque to avoid esp-idf in header)
    uint16_t fgColor_  = 0xFFFF;   // RGB565 foreground (ink → white)
    uint16_t bgColor_  = 0x0000;   // RGB565 background (paper → black)

    // Partial-flush state for flushBuffer(): keep the last pushed mono frame and
    // only re-send rows that actually changed (a moving small cursor touches
    // only a few rows → flush drops from ~52ms to a few ms). fullFlush_ forces a
    // complete send after any other draw path touched the panel.
    uint8_t* prevBuf_   = nullptr;  // last flushed 1-byte/px frame (w*h)
    bool     fullFlush_ = true;
};

} // namespace kairo::skyrizze32
