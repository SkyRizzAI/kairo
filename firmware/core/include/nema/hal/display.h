#pragma once
#include "nema/hal/driver.h"
#include <cstdint>

namespace nema {

// 1-bit monochrome display interface.
// on=true  → foreground/ink pixel (dark)
// on=false → background pixel (light/paper)
struct IDisplayDriver : IDriver {
    virtual uint16_t width()  const = 0;
    virtual uint16_t height() const = 0;
    virtual void drawPixel(uint16_t x, uint16_t y, bool on) = 0;
    virtual void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) = 0;
    virtual void clear(bool on = false) = 0;  // false = clear to background (white)
    virtual void flush() = 0;
    // XOR invert a rectangle — used for cursor highlight. Default: no-op.
    virtual void invertRect(uint16_t /*x*/, uint16_t /*y*/,
                            uint16_t /*w*/, uint16_t /*h*/) {}

    // Direct RGB565 blit — bypasses the 1-bit framebuffer for color content
    // (e.g. camera viewfinder). buf is big-endian RGB565, w*h*2 bytes.
    // Default: no-op (monochrome drivers ignore this).
    virtual void blitRgb565(const uint8_t* /*buf*/,
                             uint16_t /*x*/, uint16_t /*y*/,
                             uint16_t /*w*/, uint16_t /*h*/) {}

    // Raw buffer flush — primary hook for AsyncDisplayDriver's display task.
    // buf: row-major pixel data, 1 byte per pixel (1=ink, 0=bg), w×h bytes.
    // Default: slow-path via drawPixel + flush() — override for efficiency.
    virtual void flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) {
        for (uint16_t y = 0; y < h; y++)
            for (uint16_t x = 0; x < w; x++)
                drawPixel(x, y, buf[y * w + x] != 0);
        flush();
    }

    // Screen power signals — called by DisplayPowerManager.
    // sleep(): display just received a blank frame; signal frontend / hardware.
    // wake():  display is about to resume normal rendering.
    // Default: no-op (e-ink holds last frame without explicit sleep command).
    virtual void sleep() {}
    virtual void wake()  {}

    // Pixel density hint for auto scale-factor selection. 0 = unknown.
    // Canvas uses this to pick a logical scale when no explicit config is set.
    virtual uint16_t dpi() const { return 0; }

    // Display rotation (Plan 92 Fase A): 0/1/2/3 → 0°/90°/180°/270°. 90°/270°
    // swap width()/height() (landscape) so the resolution-independent UI reflows.
    // Default: no-op (driver doesn't support runtime rotation; it may still apply
    // a fixed rotation at init). A driver that overrides this enables live rotate.
    virtual void    setRotation(uint8_t /*r*/) {}
    virtual uint8_t rotation() const { return 0; }

    // Two-colour palette for the 1-bit framebuffer (Plan 92 Fase B). The driver
    // expands on→fg, off→bg when pushing pixels, so swapping the palette recolours
    // the whole UI. Default no-op (a true B&W panel ignores it and shows on/off).
    virtual void setPalette(uint16_t /*fgRgb565*/, uint16_t /*bgRgb565*/) {}

    // Backlight brightness 0–255 (Mission Control). Default no-op. A PWM-capable
    // backlight dims; a plain on/off backlight (skyrizz: XL9535 GPIO) maps >0→on,
    // 0→off. The UI bar works either way; the dimming depth depends on the board.
    virtual void    setBrightness(uint8_t /*level*/) {}
    virtual uint8_t brightness() const { return 255; }

    // Display colour capability (Plan 92 Fase B). false = true B&W panel (e-ink):
    // only on/off, so a colour theme is pointless (UI hides it; dark mode = invert).
    // true = the panel can show arbitrary colours (e.g. ILI9341 RGB565), so colour
    // themes (Flipper orange/black, …) are offered. The framebuffer stays 1-bit
    // either way — this just gates whether the 2 palette colours can be non-B&W.
    virtual bool supportsColor() const { return false; }
};

} // namespace nema
