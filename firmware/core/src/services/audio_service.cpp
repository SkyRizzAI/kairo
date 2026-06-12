#include "nema/services/audio_service.h"

namespace nema {

void AudioService::addInput(IAudioInput* drv, const char* id, const char* desc) {
    if (inputCount_ >= kMaxDevices) return;
    inputs_[inputCount_++] = {drv, id, desc};
}

void AudioService::addOutput(IAudioOutput* drv, const char* id, const char* desc) {
    if (outputCount_ >= kMaxDevices) return;
    outputs_[outputCount_++] = {drv, id, desc};
}

IAudioInput*  AudioService::input (int i) const { return (i >= 0 && i < inputCount_)  ? inputs_[i].drv  : nullptr; }
IAudioOutput* AudioService::output(int i) const { return (i >= 0 && i < outputCount_) ? outputs_[i].drv : nullptr; }

const char* AudioService::inputId  (int i) const { return (i >= 0 && i < inputCount_)  ? inputs_[i].id   : nullptr; }
const char* AudioService::outputId (int i) const { return (i >= 0 && i < outputCount_) ? outputs_[i].id  : nullptr; }
const char* AudioService::inputDesc(int i) const { return (i >= 0 && i < inputCount_)  ? inputs_[i].desc : nullptr; }
const char* AudioService::outputDesc(int i) const { return (i >= 0 && i < outputCount_) ? outputs_[i].desc : nullptr; }

} // namespace nema
