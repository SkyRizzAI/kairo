#pragma once
#include "nema/hal/display.h"
#include "nema/link/link_service.h"
#include <vector>

// RemoteScreenTap — IDisplayDriver decorator (Plan 35). Sits between Canvas and
// the real display: forwards every op to the inner driver (glass stays normal)
// and keeps a 1-bit shadow. On flush(), if a remote PLP session is ready, it
// RLE-encodes the shadow and streams it on the SCREEN channel. Zero-ish overhead
// when no session is connected. Taps the 1-bit buffer BEFORE any RGB conversion,
// so it is board-agnostic and the core/Canvas are unchanged.
namespace nema {

class RemoteScreenTap : public IDisplayDriver {
public:
    void init(IDisplayDriver& inner, LinkService& link);

    const char* name() const override { return "RemoteScreenTap"; }
    DriverKind  kind() const override { return DriverKind::Display; }

    uint16_t width()  const override { return inner_ ? inner_->width()  : 0; }
    uint16_t height() const override { return inner_ ? inner_->height() : 0; }

    void drawPixel(uint16_t x, uint16_t y, bool on) override;
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void clear(bool on = false) override;
    void flush() override;
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    void blitRgb565(const uint8_t* buf, uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    void flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) override;
    void sleep() override;
    void wake() override;
    uint16_t dpi() const override { return inner_ ? inner_->dpi() : 0; }

    // Re-stream the current shadow (e.g. when a viewer just connected).
    void requestResend() { streamFrame(); }

private:
    void ensureShadow();
    void streamFrame();

    IDisplayDriver*      inner_ = nullptr;
    LinkService*         link_  = nullptr;
    std::vector<uint8_t> shadow_;     // w*h, 0/1
    std::vector<uint8_t> payload_;    // scratch for SCREEN payload
    uint16_t w_ = 0, h_ = 0;
};

} // namespace nema
