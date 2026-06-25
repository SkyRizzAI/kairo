#pragma once
#include "nema/hal/audio_output.h"

namespace nema {

// WasmSpeaker — the simulator's audio output. There is no DAC in the browser, so
// the firmware synthesizes the RAW PCM the NS4168 would receive and streams those
// samples to the host page via Module.nemaAudioPcm (the same MAIN_THREAD_EM_ASM
// path WasmCableTransport uses for Module.nemaPlpOut). The Forge page
// (lib/audio/SimAudio.ts) is a dumb DAC — it plays whatever samples arrive, no
// re-synthesis. Registering this driver also lights up the Sounds settings screen
// in the simulator (it gates on caps::AudioOutput).
class WasmSpeaker : public IAudioOutput {
public:
    const char* label()     const override { return "Web Audio (browser)"; }
    float       peakLevel() const override { return 0.0f; }  // host synthesizes; no meter
    void        setVolume(float v) override { volume_ = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    void        playTone(uint16_t freqHz, uint16_t ms) override;
    void        writePcm(const int16_t* samples, size_t count, uint32_t sampleRate) override;

private:
    float volume_ = 1.0f;   // 0..1 output gain (Mission Control)
};

} // namespace nema
