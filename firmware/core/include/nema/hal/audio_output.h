#pragma once
#include <cstdint>
#include <cstddef>

namespace nema {

struct IAudioOutput {
    virtual ~IAudioOutput() = default;
    virtual const char* label()                              const = 0;
    virtual float       peakLevel()                          const = 0;  // 0.0..1.0 playback level
    virtual void        setVolume(float v)                         = 0;  // 0.0..1.0
    virtual void        playTone(uint16_t freqHz, uint16_t ms)     = 0;  // test beep

    // Play RAW 16-bit mono PCM (`count` int16 samples at `sampleRate` Hz). The
    // output reproduces these samples exactly — no re-synthesis. Default no-op so
    // tone-only outputs still compile. Backs nema.media.audioOutput.playPcm.
    virtual void        writePcm(const int16_t* samples, size_t count, uint32_t sampleRate) {
        (void)samples; (void)count; (void)sampleRate;
    }
};

} // namespace nema
