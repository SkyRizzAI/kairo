#pragma once
#include "nema/hal/led.h"
#include <vector>
#include <algorithm>

namespace nema {

// VirtualLed — a software ILed with no hardware. Stores pixel state so the
// simulator (and host tests) have a working rt.led() registry; the state is
// inspectable via pixels() for a future Forge/WASM visualisation.
class VirtualLed : public ILed {
public:
    VirtualLed(const char* label, int count, LedColorModel model = LedColorModel::Rgb)
        : label_(label), count_(count < 1 ? 1 : count), model_(model),
          px_((size_t)(count < 1 ? 1 : count) * 3, 0) {}

    // IDriver
    const char* name() const override { return "VirtualLed"; }
    DriverKind  kind() const override { return DriverKind::Other; }

    // ILed
    const char*   label()      const override { return label_; }
    int           pixelCount() const override { return count_; }
    LedColorModel colorModel() const override { return model_; }
    void setPixel(int index, uint8_t r, uint8_t g, uint8_t b) override {
        if (index < 0 || index >= count_) return;
        size_t o = (size_t)index * 3;
        px_[o] = r; px_[o + 1] = g; px_[o + 2] = b;
    }
    void setAll(uint8_t r, uint8_t g, uint8_t b) override {
        for (int i = 0; i < count_; i++) setPixel(i, r, g, b);
    }
    void clear() override { std::fill(px_.begin(), px_.end(), 0); }
    void show()  override {}   // no hardware; state lives in px_

    // Current pixel buffer (RGB triplets), for a simulator visualisation.
    const std::vector<uint8_t>& pixels() const { return px_; }

private:
    const char*          label_;
    int                  count_;
    LedColorModel        model_;
    std::vector<uint8_t> px_;
};

} // namespace nema
