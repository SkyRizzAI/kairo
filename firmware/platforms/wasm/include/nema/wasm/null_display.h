#pragma once
#include "nema/hal/display.h"
#include "nema/service.h"

namespace nema {

// Headless inner display for WASM — the RemoteScreenTap wraps it and streams the
// 1-bit buffer over PLP to Forge. There is no local glass in the browser device.
class NullDisplay : public IDisplayDriver, public IService {
public:
    NullDisplay(uint16_t w = 264, uint16_t h = 176)
        : w_(w), h_(h), nativeW_(w), nativeH_(h) {}

    const char* name() const override { return "NullDisplay"; }
    DriverKind  kind() const override { return DriverKind::Display; }
    void start() override {}
    void stop()  override {}

    uint16_t width()  const override { return w_; }
    uint16_t height() const override { return h_; }
    void drawPixel(uint16_t, uint16_t, bool) override {}
    void fillRect(uint16_t, uint16_t, uint16_t, uint16_t, bool) override {}
    void clear(bool = false) override {}
    void flush() override {}

    // Live rotation (Plan 92 Fase A): no panel hardware here, so rotating is just
    // swapping the reported logical dims — the UI reflows and RemoteScreenTap
    // ships the new W×H buffer to Forge, which adapts. Pointer events come back
    // from Forge already in the displayed (rotated) space, so no touch transform.
    void setRotation(uint8_t r) override {
        rotation_ = (uint8_t)(r & 3);
        const bool land = (rotation_ == 1 || rotation_ == 3);
        w_ = land ? nativeH_ : nativeW_;
        h_ = land ? nativeW_ : nativeH_;
    }
    uint8_t rotation() const override { return rotation_; }

    // Resize the simulated panel (sim only). Re-applies the current rotation so
    // width()/height() reflect the new native dims. Call before the screen tap
    // reads the size at boot.
    void setNativeSize(uint16_t w, uint16_t h) {
        nativeW_ = w; nativeH_ = h;
        setRotation(rotation_);
    }

    // Theme palette (Plan 92 Fase B): stored for a future Forge colourised mirror
    // (the sim streams a 1-bit buffer; Forge would apply fg/bg). No local glass.
    void setPalette(uint16_t fg, uint16_t bg) override { paletteFg_ = fg; paletteBg_ = bg; }
    uint16_t paletteFg() const { return paletteFg_; }
    uint16_t paletteBg() const { return paletteBg_; }

    // The sim represents a colour-capable device by default (Forge can render any
    // colour). setColorCapable(false) simulates a true B&W panel. (Plan 92 Fase B)
    void setColorCapable(bool on) { colorCapable_ = on; }
    bool supportsColor() const override { return colorCapable_; }

private:
    uint16_t w_, h_;
    uint16_t nativeW_, nativeH_;
    uint8_t  rotation_ = 0;
    uint16_t paletteFg_ = 0xFFFF, paletteBg_ = 0x0000;
    bool     colorCapable_ = true;
};

} // namespace nema
