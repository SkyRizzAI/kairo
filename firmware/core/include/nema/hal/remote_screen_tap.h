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

    // Rotation forwards to the inner driver; width()/height() already delegate.
    // ensureShadow() MUST run here so the shadow buffer + streamed W×H header
    // resize to the new orientation — otherwise the frame keeps the old dims and
    // the reflowed content looks clipped/"only the width changed".
    void    setRotation(uint8_t r) override { if (inner_) { inner_->setRotation(r); ensureShadow(); } }
    uint8_t rotation() const override { return inner_ ? inner_->rotation() : 0; }
    // Plan 92 Fase B — track the theme palette and push it to the host on the SYSTEM
    // channel when it changes, so the Forge mirror (sim + USB remote) colourises from
    // the DEVICE's colour/dark setting instead of a manual web selection.
    void    setPalette(uint16_t fg, uint16_t bg) override;
    bool    supportsColor() const override { return inner_ ? inner_->supportsColor() : false; }

    // Re-stream the current shadow + palette (e.g. when a viewer just connected).
    // Clears prev_ so the (otherwise unchanged) frame is force-sent to the new viewer.
    void requestResend() { prev_.clear(); streamFrame(); sendPalette(); }

    // Downsample the streamed frame by an integer factor before RLE (Plan 93). Set this
    // to the UI scale (e.g. 2 for a 2× UI): because every logical pixel is drawn as an
    // f×f block of identical physical pixels, dropping to 1 sample per block is LOSSLESS
    // and cuts the streamed bytes by f² — e.g. 320×240 → 160×120 is 4× smaller. The host
    // gets the logical resolution in the W×H header and scales it to its viewport.
    void setDownscale(int f) { downscale_ = f < 1 ? 1 : f; }

private:
    void ensureShadow();
    void streamFrame();
    void sendPalette();   // SYSTEM [SetPalette][fg:2][bg:2] (Plan 92 Fase B)

    IDisplayDriver*      inner_ = nullptr;
    LinkService*         link_  = nullptr;
    std::vector<uint8_t> shadow_;     // w*h, 0/1
    std::vector<uint8_t> payload_;    // scratch for SCREEN payload
    std::vector<uint8_t> ds_;         // scratch for the downsampled frame
    std::vector<uint8_t> prev_;       // last frame actually sent (skip resend if unchanged)
    int                  downscale_ = 1;
    uint16_t w_ = 0, h_ = 0;
    uint16_t palFg_ = 0xFFFF, palBg_ = 0x0000;   // last palette pushed to the host
};

} // namespace nema
