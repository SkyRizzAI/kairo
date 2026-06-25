#include "nema/skyrizze32/i2s_speaker.h"
#include "nema/skyrizze32/es7243e_mic.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <driver/i2s.h>   // legacy I2S API — shared I2S0 with Es7243eMic
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <vector>

namespace nema::skyrizze32 {

void I2sSpeaker::start() {
    if (rt_) rt_->log().info("I2sSpeaker", "NS4168 ready (shared I2S0 TX, GPIO45)");
}

// Beep task params — small struct passed via xTaskCreate pvParameters.
// Writes go to the shared legacy I2S0 port (installed by Es7243eMic).
struct BeepParams {
    uint16_t freqHz;
    uint16_t ms;
    float    volume;   // 0..1 output gain (Mission Control)
};

// FreeRTOS task: runs beep in background so GUI thread is not blocked
static void beepTask(void* arg) {
    BeepParams* p = static_cast<BeepParams*>(arg);

    // One cycle of square wave at freqHz, 16 kHz, 32-bit stereo.
    // Square wave avoids sinf (no FPU dependency) and sounds distinct.
    const uint32_t sampleRate      = 16000;
    const uint32_t samplesPerCycle = (p->freqHz > 0) ? (sampleRate / p->freqHz) : 36;
    const uint32_t totalFrames     = (uint32_t)((uint64_t)sampleRate * p->ms / 1000);
    const int32_t  hi              =  (int32_t)(1073741824.0f * p->volume);  // scaled by volume
    const int32_t  lo              = -hi;

    // Stack-local cycle buffer (max ~72 frames for 220 Hz at 16 kHz = 144 samples)
    // Worst case: 20 Hz → 800 samples × 8 bytes = 6.4 KB — use heap for safety
    const size_t cycleStereo = samplesPerCycle * 2;  // L + R per frame
    int32_t* buf = static_cast<int32_t*>(pvPortMalloc(cycleStereo * sizeof(int32_t)));
    if (!buf) { delete p; vTaskDelete(nullptr); return; }

    for (uint32_t i = 0; i < samplesPerCycle; i++) {
        int32_t s = (i < samplesPerCycle / 2) ? hi : lo;
        buf[i * 2]     = s;
        buf[i * 2 + 1] = s;
    }

    uint32_t framesWritten = 0;
    while (framesWritten < totalFrames) {
        uint32_t remaining = totalFrames - framesWritten;
        uint32_t chunk     = (remaining < samplesPerCycle) ? remaining : samplesPerCycle;
        size_t   bytesOut  = 0;
        // beepTask is a free FreeRTOS task (no `rt_`/Logger in scope), so any
        // diagnostics here would have to be plumbed through BeepParams; the
        // bring-up printf has served its purpose and is intentionally gone.
        if (i2s_write(I2S_NUM_0, buf, chunk * 2 * sizeof(int32_t),
                      &bytesOut, pdMS_TO_TICKS(200)) != ESP_OK) break;
        framesWritten += chunk;
    }

    // Write a short silence so NS4168 doesn't end on a DC offset
    int32_t silence[64] = {};
    size_t dummy = 0;
    i2s_write(I2S_NUM_0, silence, sizeof(silence), &dummy, pdMS_TO_TICKS(50));

    vPortFree(buf);
    delete p;
    vTaskDelete(nullptr);
}

void I2sSpeaker::playTone(uint16_t freqHz, uint16_t ms) {
    if (!mic_) {
        if (rt_) rt_->log().warn("I2sSpeaker", "no mic reference");
        return;
    }
    if (!mic_->i2sReady()) {
        if (rt_) rt_->log().warn("I2sSpeaker", "I2S not ready — mic not started?");
        return;
    }

    if (rt_) rt_->log().info("I2sSpeaker", "playTone",
        {{"hz", std::to_string(freqHz)}, {"ms", std::to_string(ms)}});

    // Spawn background task — keeps GUI thread responsive during the beep
    auto* params  = new BeepParams{freqHz, ms, volume_};
    BaseType_t ok = xTaskCreate(beepTask, "beep", 4096, params,
                                5, nullptr);
    if (ok != pdPASS) {
        if (rt_) rt_->log().error("I2sSpeaker", "beep task create failed");
        delete params;
    }
}

// Raw PCM playback (media.audioOutput.playPcm). Runs in a background task so the
// caller (JS engine / GUI thread) is not blocked. Owns a copy of the samples.
struct PcmParams {
    std::vector<int16_t> samples;
    float                volume;
};

static void pcmTask(void* arg) {
    PcmParams* p = static_cast<PcmParams*>(arg);

    // int16 mono → 32-bit stereo (NS4168 reads the upper bits), scaled by volume.
    constexpr size_t kChunk = 256;                 // frames per i2s_write
    int32_t buf[kChunk * 2];
    size_t i = 0;
    while (i < p->samples.size()) {
        size_t n = 0;
        while (n < kChunk && i < p->samples.size()) {
            // int16 → upper bits of a 32-bit slot. Multiply (not <<16) to avoid
            // signed-shift UB on negative samples; 32767*65536 still fits int32.
            int32_t s = (int32_t)(p->samples[i] * p->volume) * 65536;
            buf[n * 2]     = s;
            buf[n * 2 + 1] = s;
            n++; i++;
        }
        size_t bytesOut = 0;
        if (i2s_write(I2S_NUM_0, buf, n * 2 * sizeof(int32_t),
                      &bytesOut, pdMS_TO_TICKS(200)) != ESP_OK) break;
    }

    int32_t silence[64] = {};
    size_t dummy = 0;
    i2s_write(I2S_NUM_0, silence, sizeof(silence), &dummy, pdMS_TO_TICKS(50));

    delete p;
    vTaskDelete(nullptr);
}

void I2sSpeaker::writePcm(const int16_t* samples, size_t count, uint32_t /*sampleRate*/) {
    if (!samples || count == 0) return;
    if (!mic_ || !mic_->i2sReady()) {
        if (rt_) rt_->log().warn("I2sSpeaker", "writePcm: I2S not ready");
        return;
    }
    // sampleRate is advisory — the shared I2S0 clock is fixed at 16 kHz.
    auto* params  = new PcmParams{std::vector<int16_t>(samples, samples + count), volume_};
    BaseType_t ok = xTaskCreate(pcmTask, "pcm", 4096, params, 5, nullptr);
    if (ok != pdPASS) {
        if (rt_) rt_->log().error("I2sSpeaker", "pcm task create failed");
        delete params;
    }
}

} // namespace nema::skyrizze32
