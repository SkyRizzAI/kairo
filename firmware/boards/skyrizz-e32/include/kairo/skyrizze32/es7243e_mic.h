#pragma once
#include "kairo/hal/audio_input.h"
#include "kairo/service.h"

namespace kairo { class Runtime; }
namespace kairo::skyrizze32 { class Xl9535; }

namespace kairo::skyrizze32 {

class Es7243eMic : public kairo::IAudioInput, public kairo::IService {
public:
    void init(kairo::Runtime& rt, Xl9535& expander);

    // IAudioInput
    const char* label()      const override { return "Built-in Mic (ES7243E)"; }
    float       peakLevel()  const override { return peak_; }
    void        startCapture()     override;
    void        stopCapture()      override;

    // IService
    const char* name() const override { return "Es7243eMic"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t nowMs) override;

    // TX handle exposed for I2sSpeaker (NS4168 shares same I2S bus).
    // Valid only after start(). Opaque void* to avoid ESP-IDF in header.
    void* txHandle() const { return i2sTxHandle_; }

private:
    void i2sInit();
    bool i2cInit();

    kairo::Runtime* rt_         = nullptr;
    Xl9535*         expander_   = nullptr;
    void*           i2sRxHandle_ = nullptr;  // i2s_chan_handle_t — mic RX
    void*           i2sTxHandle_ = nullptr;  // i2s_chan_handle_t — speaker TX (NS4168)
    float           peak_        = 0.0f;
    bool            capturing_   = false;
};

} // namespace kairo::skyrizze32
