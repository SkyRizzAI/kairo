#pragma once
#include "nema/hal/driver.h"
#include <cstdint>

namespace nema {

// Colour model a physical LED supports. A board declares this per-LED so the UI
// and apps can adapt (an RGB effect degrades to on/off blink on a mono LED).
enum class LedColorModel : uint8_t { Mono, Rgb };

// ILed — low-level per-LED (or LED-strip) HAL. ONE physical addressable unit:
// a single indicator LED, or a WS2812 chain with `pixelCount()` pixels.
//
// Deliberately dumb: set pixel colours + show(). All timing/patterns (blink,
// pulse, notification effects) live in LedService, which drives ILed on tick —
// so drivers stay trivial and the effect engine is shared + board-agnostic.
// A board can register several ILed instances (rt.led() is a multi-registry).
struct ILed : IDriver {
    virtual const char*  label()      const = 0;   // "Status", "Ring", …
    virtual int          pixelCount() const = 0;   // ≥1 (WS2812 chain length)
    virtual LedColorModel colorModel() const = 0;  // Mono → r/g/b collapsed to brightness

    // Set one pixel (0..pixelCount-1). Components are 0..255. On a Mono LED the
    // driver uses max(r,g,b) as brightness. No effect until show().
    virtual void setPixel(int index, uint8_t r, uint8_t g, uint8_t b) = 0;
    // Set every pixel to one colour (no effect until show()).
    virtual void setAll(uint8_t r, uint8_t g, uint8_t b) = 0;
    // All pixels off (no effect until show()).
    virtual void clear() = 0;
    // Push the pixel buffer to the hardware.
    virtual void show() = 0;

    // Optional global brightness scale (0..255). Default no-op for drivers that
    // fold brightness into the colour values themselves.
    virtual void setBrightness(uint8_t /*level*/) {}
};

} // namespace nema
