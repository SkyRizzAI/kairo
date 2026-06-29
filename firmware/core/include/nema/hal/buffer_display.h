#pragma once
#include "nema/hal/display.h"
#include "nema/hal/mono1.h"
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace nema {

// BufferDisplay — an IDisplayDriver backed by a caller-owned memory buffer.
// Lets an app render with the exact same Canvas API as the real screen, but
// into RAM. The app thread then hands the buffer to the GUI thread (AppHost).
// flush() is a no-op: handoff is done explicitly by the app via present().
//
// Plan 97 P3b — the buffer is 1-bit PACKED (nema::mono1 layout), not 1 byte/pixel:
// the UI content is mono everywhere (color is a driver-side 2-colour palette), so
// a 320×240 frame is ~9.6 KB instead of ~76.8 KB. byteSize() = how many bytes the
// caller must allocate. (A future RGB UI mode would use an RGB565 buffer instead;
// that is an additive path — see mono1.h.)
class BufferDisplay : public IDisplayDriver {
public:
    BufferDisplay(uint8_t* buf, uint16_t w, uint16_t h) : buf_(buf), w_(w), h_(h) {}

    // Bytes the backing buffer must hold for a w×h frame.
    static size_t byteSize(uint16_t w, uint16_t h) { return nema::mono1::byteSize(w, h); }

    const char* name() const override { return "BufferDisplay"; }
    DriverKind  kind() const override { return DriverKind::Display; }

    uint16_t width()  const override { return w_; }
    uint16_t height() const override { return h_; }

    void drawPixel(uint16_t x, uint16_t y, bool on) override {
        if (x < w_ && y < h_) nema::mono1::set(buf_, w_, x, y, on);
    }
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override {
        for (uint16_t r = y; r < y + h && r < h_; r++)
            for (uint16_t c = x; c < x + w && c < w_; c++)
                nema::mono1::set(buf_, w_, c, r, on);
    }
    void clear(bool on = false) override {
        std::memset(buf_, on ? 0xFF : 0x00, nema::mono1::byteSize(w_, h_));
    }
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override {
        for (uint16_t r = y; r < y + h && r < h_; r++)
            for (uint16_t c = x; c < x + w && c < w_; c++)
                nema::mono1::flip(buf_, w_, c, r);
    }
    void flush() override {}  // no-op — app calls AppContext::present() instead

private:
    uint8_t* buf_;
    uint16_t w_, h_;
};

} // namespace nema
