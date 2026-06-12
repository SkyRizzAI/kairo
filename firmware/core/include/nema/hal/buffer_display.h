#pragma once
#include "nema/hal/display.h"
#include <cstdint>
#include <cstddef>

namespace nema {

// BufferDisplay — an IDisplayDriver backed by a caller-owned memory buffer.
// Lets an app render with the exact same Canvas API as the real screen, but
// into RAM. The app thread then hands the buffer to the GUI thread (AppHost).
// flush() is a no-op: handoff is done explicitly by the app via present().
class BufferDisplay : public IDisplayDriver {
public:
    BufferDisplay(uint8_t* buf, uint16_t w, uint16_t h) : buf_(buf), w_(w), h_(h) {}

    const char* name() const override { return "BufferDisplay"; }
    DriverKind  kind() const override { return DriverKind::Display; }

    uint16_t width()  const override { return w_; }
    uint16_t height() const override { return h_; }

    void drawPixel(uint16_t x, uint16_t y, bool on) override {
        if (x < w_ && y < h_) buf_[(size_t)y * w_ + x] = on ? 1 : 0;
    }
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override {
        for (uint16_t r = y; r < y + h && r < h_; r++)
            for (uint16_t c = x; c < x + w && c < w_; c++)
                buf_[(size_t)r * w_ + c] = on ? 1 : 0;
    }
    void clear(bool on = false) override {
        for (size_t i = 0; i < (size_t)w_ * h_; i++) buf_[i] = on ? 1 : 0;
    }
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override {
        for (uint16_t r = y; r < y + h && r < h_; r++)
            for (uint16_t c = x; c < x + w && c < w_; c++)
                buf_[(size_t)r * w_ + c] ^= 1;
    }
    void flush() override {}  // no-op — app calls AppContext::present() instead

private:
    uint8_t* buf_;
    uint16_t w_, h_;
};

} // namespace nema
