#pragma once
#include "kairo/hal/audio_output.h"
#include "kairo/service.h"

namespace kairo { class Runtime; }
namespace kairo::skyrizze32 { class Es7243eMic; }

namespace kairo::skyrizze32 {

// I2sSpeaker — NS4168 I2S Class D amplifier (GPIO45).
// Shares I2S0 full-duplex channel with Es7243eMic (same BCLK/WS).
// init() must be called AFTER Es7243eMic::init(), and start() runs after
// Es7243eMic::start() so the TX handle is already created.
class I2sSpeaker : public kairo::IAudioOutput, public kairo::IService {
public:
    void init(kairo::Runtime& rt, Es7243eMic& mic) { rt_ = &rt; mic_ = &mic; }

    // IAudioOutput
    const char* label()     const override { return "NS4168 Speaker"; }
    float       peakLevel() const override { return 0.0f; }
    void        setVolume(float)   override {}
    void        playTone(uint16_t freqHz, uint16_t ms) override;

    // IService
    const char* name() const override { return "I2sSpeaker"; }
    void start() override;
    void stop()  override {}
    void tick(uint64_t) override {}

private:
    kairo::Runtime*          rt_  = nullptr;
    Es7243eMic*              mic_ = nullptr;
};

} // namespace kairo::skyrizze32
