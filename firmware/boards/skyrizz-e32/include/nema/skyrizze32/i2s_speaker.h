#pragma once
#include "nema/hal/audio_output.h"
#include "nema/service.h"

namespace nema { class Runtime; }
namespace nema::skyrizze32 { class Es7243eMic; }

namespace nema::skyrizze32 {

// I2sSpeaker — NS4168 I2S Class D amplifier (GPIO45).
// Shares I2S0 full-duplex channel with Es7243eMic (same BCLK/WS).
// init() must be called AFTER Es7243eMic::init(), and start() runs after
// Es7243eMic::start() so the TX handle is already created.
class I2sSpeaker : public nema::IAudioOutput, public nema::IService {
public:
    void init(nema::Runtime& rt, Es7243eMic& mic) { rt_ = &rt; mic_ = &mic; }

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
    nema::Runtime*          rt_  = nullptr;
    Es7243eMic*              mic_ = nullptr;
};

} // namespace nema::skyrizze32
