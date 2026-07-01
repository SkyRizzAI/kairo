#pragma once
#include "nema/hal/led.h"
#include <cstdint>
#include <vector>

namespace nema {

// Esp32Ws2812 — WS2812/NeoPixel chain driver over the ESP32-S3 RMT peripheral.
//
// Implements the core ILed HAL for a chain of `count` RGB pixels on one GPIO.
// Zero external components: uses driver/rmt_tx.h directly with a bytes encoder
// programmed to WS2812 bit timings. setPixel/setAll write a GRB buffer; show()
// shifts it out. begin() must succeed before use (registers the RMT channel).
class Esp32Ws2812 : public ILed {
public:
    Esp32Ws2812(int gpio, int count);

    bool begin();   // create RMT channel + encoder; false on failure

    // IDriver
    const char* name() const override { return "WS2812"; }
    DriverKind  kind() const override { return DriverKind::Other; }

    // ILed
    const char*   label()      const override { return "WS2812"; }
    int           pixelCount() const override { return count_; }
    LedColorModel colorModel() const override { return LedColorModel::Rgb; }
    void setPixel(int index, uint8_t r, uint8_t g, uint8_t b) override;
    void setAll(uint8_t r, uint8_t g, uint8_t b) override;
    void clear() override;
    void show() override;
    void setBrightness(uint8_t level) override { bright_ = level; }

private:
    int                  gpio_;
    int                  count_;
    uint8_t              bright_ = 255;
    std::vector<uint8_t> buf_;      // GRB per pixel (count_*3)
    void*                chan_  = nullptr;   // rmt_channel_handle_t
    void*                enc_   = nullptr;   // rmt_encoder_handle_t
    bool                 ready_ = false;
};

} // namespace nema
