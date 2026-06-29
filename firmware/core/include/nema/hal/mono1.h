#pragma once
#include <cstdint>
#include <cstddef>

// 1-bit packed monochrome framebuffer helpers (Plan 97 P3b).
//
// Pixel (x,y) maps to the CONTIGUOUS bit index `y*w + x`, MSB-first within each
// byte (bit 7 = the lowest index). This is the exact layout the LcdDriver already
// uses for its internal framebuffer (drawPixel/flush), so packing the app buffer
// the same way unifies the format rather than introducing a new one.
//
// Storage: ceil(w*h / 8) bytes. For panels whose width is a multiple of 8 (240,
// 320, 128, …) rows are also byte-aligned, which keeps the LcdDriver row-diff
// cheap; the helpers themselves work for any width.
//
// FORWARD-COMPAT (future RGB UI mode, Plan 92): a full-colour app would use an
// RGB565 buffer (16 bpp) instead. That is an additive PixelFormat::Rgb565 path —
// these Mono1 helpers stay the default and are not in its way. Today the whole UI
// is a 1-bit content layer recoloured by the driver's 2-colour palette (fg/bg),
// so Mono1 is correct for both true-B&W and palette-colour panels.
namespace nema::mono1 {

// Bytes needed to store a w×h 1-bit frame.
inline size_t byteSize(uint16_t w, uint16_t h) {
    return ((size_t)w * h + 7) / 8;
}

// Set/clear pixel (x,y). Caller bounds-checks x<w, y<h.
inline void set(uint8_t* buf, uint16_t w, uint16_t x, uint16_t y, bool on) {
    size_t  idx  = (size_t)y * w + x;
    uint8_t mask = (uint8_t)(0x80 >> (idx & 7));
    if (on) buf[idx >> 3] |=  mask;
    else    buf[idx >> 3] &= (uint8_t)~mask;
}

// Read pixel (x,y) → on/off.
inline bool get(const uint8_t* buf, uint16_t w, uint16_t x, uint16_t y) {
    size_t idx = (size_t)y * w + x;
    return (buf[idx >> 3] >> (7 - (idx & 7))) & 1;
}

// XOR (invert) pixel (x,y).
inline void flip(uint8_t* buf, uint16_t w, uint16_t x, uint16_t y) {
    size_t idx = (size_t)y * w + x;
    buf[idx >> 3] ^= (uint8_t)(0x80 >> (idx & 7));
}

} // namespace nema::mono1
