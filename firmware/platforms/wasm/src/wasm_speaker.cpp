#include "nema/wasm/wasm_speaker.h"
#include <emscripten.h>
#include <vector>
#include <cstdint>

namespace nema {

// The simulator speaker is a "dumb DAC": the firmware synthesizes the actual PCM
// the NS4168 would receive and streams those RAW samples to the host page, which
// just plays whatever arrives (see SimAudio.ts). Nothing is re-synthesized in the
// browser — exactly like a real speaker fed from I2S. 16 kHz mono int16 matches
// the device audio path (i2s_speaker.cpp builds a 16 kHz square wave).
void WasmSpeaker::playTone(uint16_t freqHz, uint16_t ms) {
    constexpr int kSampleRate = 16000;
    const uint32_t totalSamples = (uint32_t)((uint64_t)kSampleRate * ms / 1000);
    if (totalSamples == 0) return;

    // Square wave (matches the device NS4168 test beep — no sinf, FPU-free).
    const uint32_t spc = (freqHz > 0) ? (kSampleRate / freqHz) : 36;   // samples/cycle
    // 0..1 volume → int16, with headroom (square waves are loud).
    const int16_t amp = (int16_t)(volume_ * 0.25f * 32767.0f);

    std::vector<int16_t> pcm(totalSamples);
    for (uint32_t i = 0; i < totalSamples; i++)
        pcm[i] = ((i % spc) < spc / 2) ? amp : (int16_t)(-amp);

    writePcm(pcm.data(), pcm.size(), kSampleRate);
}

void WasmSpeaker::writePcm(const int16_t* samples, size_t count, uint32_t sampleRate) {
    if (!samples || count == 0) return;
    // Stream the raw PCM to the host main thread. MAIN_THREAD_EM_ASM is
    // synchronous + proxied, so HEAP16.slice copies the samples before they go out
    // of scope. Same shared-heap pattern as WasmCableTransport::send. The host
    // installed Module.nemaAudioPcm (VirtualCableTransport); it gets a typed-array
    // copy + the sample rate and feeds it straight to Web Audio — no re-synthesis.
    MAIN_THREAD_EM_ASM({
        if (Module && Module.nemaAudioPcm)
            Module.nemaAudioPcm(HEAP16.slice($0 >> 1, ($0 >> 1) + $1), $2);
    }, samples, (int)count, (int)sampleRate);
}

} // namespace nema
