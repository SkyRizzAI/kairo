#pragma once
#include "nema/hal/display.h"
#include "nema/service.h"

namespace nema {

// Headless inner display for WASM — the RemoteScreenTap wraps it and streams the
// 1-bit buffer over KLP to Forge. There is no local glass in the browser device.
class NullDisplay : public IDisplayDriver, public IService {
public:
    NullDisplay(uint16_t w = 264, uint16_t h = 176) : w_(w), h_(h) {}

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

private:
    uint16_t w_, h_;
};

} // namespace nema
