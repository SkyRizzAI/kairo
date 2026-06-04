#pragma once
#include <cstdint>

namespace kairo {

struct IAudioOutput {
    virtual ~IAudioOutput() = default;
    virtual const char* label()                              const = 0;
    virtual float       peakLevel()                          const = 0;  // 0.0..1.0 playback level
    virtual void        setVolume(float v)                         = 0;  // 0.0..1.0
    virtual void        playTone(uint16_t freqHz, uint16_t ms)     = 0;  // test beep
};

} // namespace kairo
