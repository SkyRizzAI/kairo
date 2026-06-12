#pragma once
#include "nema/hal/audio_input.h"
#include "nema/service.h"

namespace nema { class Runtime; }
namespace nema::skyrizze32 { class Xl9535; }

namespace nema::skyrizze32 {

class Es7243eMic : public nema::IAudioInput, public nema::IService {
public:
    void init(nema::Runtime& rt, Xl9535& expander);

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

    // I2S readiness for I2sSpeaker. Mic owns the shared legacy I2S0 driver
    // (TX→NS4168 + RX←ES7243E installed together); the speaker writes to the
    // same port (I2S_NUM_0). True only after start() installs the driver.
    bool i2sReady() const { return i2sInstalled_; }

private:
    void i2sInit();
    bool i2cInit();

    nema::Runtime* rt_         = nullptr;
    Xl9535*         expander_   = nullptr;
    bool            i2sInstalled_ = false;  // legacy I2S0 driver installed
    float           peak_        = 0.0f;
    bool            capturing_   = false;
};

} // namespace nema::skyrizze32
