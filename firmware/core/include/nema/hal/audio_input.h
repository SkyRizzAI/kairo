#pragma once

namespace nema {

// IAudioInput — microphone / line-in abstraction.
// Drivers update peakLevel() in their tick() (IService).
struct IAudioInput {
    virtual ~IAudioInput() = default;
    virtual const char* label()       const = 0;  // "Built-in Mic", "Line In"
    virtual float       peakLevel()   const = 0;  // 0.0..1.0, RMS peak, updated per tick
    virtual void        startCapture()      = 0;
    virtual void        stopCapture()       = 0;
};

} // namespace nema
