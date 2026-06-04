#pragma once
#include "kairo/hal/audio_input.h"
#include "kairo/hal/audio_output.h"

namespace kairo {

class AudioService {
public:
    static constexpr int kMaxDevices = 8;

    void addInput (IAudioInput*,  const char* id, const char* desc);
    void addOutput(IAudioOutput*, const char* id, const char* desc);

    int           inputCount()  const { return inputCount_; }
    int           outputCount() const { return outputCount_; }
    IAudioInput*  input (int i) const;
    IAudioOutput* output(int i) const;
    const char*   inputId  (int i) const;
    const char*   outputId (int i) const;
    const char*   inputDesc(int i) const;
    const char*   outputDesc(int i) const;

private:
    struct InputEntry  { IAudioInput*  drv; const char* id; const char* desc; };
    struct OutputEntry { IAudioOutput* drv; const char* id; const char* desc; };

    InputEntry  inputs_ [kMaxDevices] = {};
    OutputEntry outputs_[kMaxDevices] = {};
    int         inputCount_  = 0;
    int         outputCount_ = 0;
};

} // namespace kairo
