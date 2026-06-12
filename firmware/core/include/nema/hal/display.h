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
};

} // namespace nema
