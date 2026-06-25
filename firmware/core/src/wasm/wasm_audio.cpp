// Plan 86 + ADR 0016 — audio.* host imports: tone + volume for WASM apps.
//
// Thin bridge only: forwards to the kernel's audio output (`rt.audio()`), which
// the sound manager routes to the active board's speaker (I2S on SkyRizz) or the
// simulator's Web Audio. Whether the board has an output, and how it's routed, is
// decided there — the app just asks for a tone; on a board with no output the
// calls are silently ignored (outputCount() == 0).
//
// playPcm is intentionally NOT exposed to WASM-C apps (binary input param — the
// WASM-C ABI treats those as opaque; it stays JS-only, see ADR 0016).
#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "nema/runtime.h"
#include "nema/services/audio_service.h"
#include "wasm3.h"
#include "m3_env.h"

namespace nema {
namespace {

static WasmHostCtx* hostOf(IM3Runtime rt) {
    return static_cast<WasmHostCtx*>(m3_GetUserData(rt));
}

// ── audio.audio_play_tone(freq_hz, ms) ────────────────────────────────────────
m3ApiRawFunction(wasm_audio_play_tone) {
    m3ApiGetArg(int32_t, freq);
    m3ApiGetArg(int32_t, ms);
    WasmHostCtx* h = hostOf(runtime);
    if (h && h->ctx) {
        auto& a = h->ctx->runtime().audio();
        if (a.outputCount() > 0)
            if (auto* o = a.output(0))
                o->playTone((uint16_t)(freq < 0 ? 0 : freq), (uint16_t)(ms < 0 ? 0 : ms));
    }
    m3ApiSuccess();
}

// ── audio.audio_set_volume(level 0..100) ──────────────────────────────────────
m3ApiRawFunction(wasm_audio_set_volume) {
    m3ApiGetArg(int32_t, level);
    if (level < 0)   level = 0;
    if (level > 100) level = 100;
    WasmHostCtx* h = hostOf(runtime);
    if (h && h->ctx) {
        auto& a = h->ctx->runtime().audio();
        for (int i = 0; i < a.outputCount(); i++)
            if (auto* o = a.output(i)) o->setVolume(level / 100.0f);
    }
    m3ApiSuccess();
}

} // anon namespace

void linkAudioImports(IM3Module mod) {
    m3_LinkRawFunction(mod, "audio", "audio_play_tone",  "v(ii)", &wasm_audio_play_tone);
    m3_LinkRawFunction(mod, "audio", "audio_set_volume", "v(i)",  &wasm_audio_set_volume);
}

} // namespace nema
