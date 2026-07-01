#pragma once
#include "nema/hal/display.h"
#include "nema/service.h"
#include <cstdint>
#include <cstddef>

namespace nema {
class Runtime;
}

namespace nema::skyrizze32 {

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
    void sleep() override { setBacklight(false); }
    void wake()  override { setBacklight(true);  }
    // Direct RGB565 blit — bypasses 1-bit framebuf for camera/color content.
    // Handles panel inversion (inverts each pixel before SPI write).
    void blitRgb565(const uint8_t* buf,
                    uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h) override;

    // Live rotation (Plan 92 Fase A): swap logical dims + re-send MADCTL so the
    // panel re-scans the (same-size) framebuffer. fullFlush_ forces a full repaint.
    void    setRotation(uint8_t r) override;
    uint8_t rotation() const override { return rotation_; }

    // Theme palette (Plan 92 Fase B): the 1-bit framebuffer expands on→fg, off→bg.
    // chunkbuf_ is sent little-endian but the panel reads each pixel big-endian, so
    // an RGB565 value arrives byte-swapped (orange 0xFB20 → 0x20FB → blue/green).
    // Byte-swap here so the panel receives the true colour. Symmetric colours
    // (white 0xFFFF / black 0x0000) are swap-invariant and unaffected.
    void setPalette(uint16_t fg, uint16_t bg) override {
        auto bswap = [](uint16_t c) -> uint16_t { return (uint16_t)((c >> 8) | (c << 8)); };
        uint16_t nf = bswap(fg), nb = bswap(bg);
        // Plan 98: the diffing pushMono() compares 1-bit content only. A palette
        // change recolours the SAME bits, so force one full repaint or the theme
        // switch would leave stale colours on unchanged rows.
        if (nf != fgColor_ || nb != bgColor_) { setColors(nf, nb); fullFlush_ = true; }
    }
    // ILI9341 is an RGB565 panel → colour themes are available.
    bool supportsColor() const override { return true; }

    // Backlight is a plain XL9535 GPIO (no PWM) → brightness maps >0→on, 0→off.
    void    setBrightness(uint8_t level) override;
    uint8_t brightness() const override { return brightness_; }

    // Fast full-screen blit of a 1-bit packed mono buffer (nema::mono1) straight to
    // the panel in one diffed pass — skips the per-pixel drawPixel loop AND the 1-bit
    // framebuffer. Used by AppHost for unscaled fullscreen apps. Assumes w==width_.
    void flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) override;

    // Plan 98: scaled fast path — expand a LOGICAL 1-bit buffer (lw×lh) by an integer
    // `scale` to fill the panel, in one pass. Used by AppHost for fullscreen apps at
    // UI scale >1 (where the buffer is logical, so flushBuffer would no-op). Returns
    // true (handled). Full push (no diff) → forces the next mono flush to full.
    bool flushBufferScaled(const uint8_t* buf, uint16_t lw, uint16_t lh,
                           uint8_t scale) override;

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
    void applyMadctl();   // send CMD_MADCTL for the current rotation_ (panel must be up)
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void spiWrite(uint8_t* data, size_t len, bool isData);
    void pushMono(const uint8_t* buf);   // Plan 98: shared diffing push (flush + flushBuffer)

    Runtime* rt_       = nullptr;
    Xl9535*  expander_ = nullptr;
    // ILI9341 native portrait orientation on this board (matches Rust reference).
    uint16_t width_    = 240;
    uint16_t height_   = 320;
    // Display rotation (Plan 92 Fase A): 0/1/2/3 → 0°/90°/180°/270°. 90°/270°
    // swap width_/height_ (landscape); applyMadctl() sets the matching MADCTL.
    // nativeW_/nativeH_ hold the portrait dims so setRotation() can recompute.
    uint16_t nativeW_  = 240;
    uint16_t nativeH_  = 320;
    uint8_t  rotation_ = 0;
    uint8_t  brightness_ = 255;   // Mission Control (on/off on this board)

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

} // namespace nema::skyrizze32
