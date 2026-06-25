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
    // Output gain 0.0–1.0 (Mission Control). Scales the tone amplitude — the only
    // playback path on this board is playTone().
    void        setVolume(float v) override { volume_ = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    float       volume() const { return volume_; }
    void        playTone(uint16_t freqHz, uint16_t ms) override;
    // Raw 16-bit mono PCM → I2S (upconverted to 32-bit stereo). The I2S clock is
    // fixed at 16 kHz, so sampleRate is advisory only. Backs media.audioOutput.playPcm.
    void        writePcm(const int16_t* samples, size_t count, uint32_t sampleRate) override;

    // IService
    const char* name() const override { return "I2sSpeaker"; }
    void start() override;
    void stop()  override {}
    void tick(uint64_t) override {}

private:
    nema::Runtime*          rt_  = nullptr;
    Es7243eMic*              mic_ = nullptr;
    float                    volume_ = 1.0f;   // output gain 0..1 (Mission Control)
};

} // namespace nema::skyrizze32
