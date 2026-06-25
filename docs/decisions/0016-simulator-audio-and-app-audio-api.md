# 0016 — Simulator audio bridge (raw PCM) + app-facing audio output API

- **Status:** Accepted
- **Date:** 2026-06-26
- **Area:** platforms/wasm, core/media-api, idl/codegen

## Context

The SkyRizz E32 speaker (NS4168) produced no sound. Investigation (see
`docs/plans/32-media-hardware.md`) proved the firmware path correct — full bytes
written, clocks live, amp always-on — and pointed at hardware (the speaker is a
hand-attached part on the `SP1` JST connector, not machine-placed). Rather than
stay blocked on hardware, we moved audio bring-up to the **simulator**, where the
browser can actually produce sound.

Two needs drove the design:

1. The simulator had no audio output at all, so **Settings → Sounds** was hidden
   (it gates on `caps::AudioInput || caps::AudioOutput`) and the test beep was
   silent in Forge.
2. Audio should be **usable by custom apps**, not just the built-in Sounds screen.
   The `nema:media` IDL only declared `list()` (device enumeration), marked
   `@future` and unwired.

A key requirement surfaced in review: audio must be **raw**, not an RPC. An early
attempt bridged `playTone(freq, ms)` as a high-level call the browser
re-synthesized with a Web Audio oscillator. That is not dynamic — it only ever
works for tones, and the browser, not the firmware, decides the waveform.

## Decision

**1. Raw PCM streaming, not RPC.** The firmware synthesizes the actual PCM the
NS4168 would receive and streams those samples to the host; the browser is a dumb
DAC that plays whatever arrives. `WasmSpeaker` (firmware/platforms/wasm) forwards
samples via `MAIN_THREAD_EM_ASM → Module.nemaAudioPcm(Int16Array, sampleRate)` —
the same shared-heap pattern `WasmCableTransport::send` uses for
`Module.nemaPlpOut`. `SimAudio.ts` (packages/forge) converts int16 → `AudioBuffer`
and schedules chunks back-to-back on a shared cursor (seamless streaming). The
HAL gained `IAudioOutput::writePcm(samples, count, sampleRate)` (default no-op),
implemented by `WasmSpeaker` (stream) and `I2sSpeaker` (I2S → NS4168, background
task). `playTone` is now just a PCM generator that calls `writePcm`.

**2. Audio output exposed to apps via `nema:media`.** `api/media.pidl`
`audio-output` gained `set-volume(u8)`, `play-tone(u16,u16)`, and
`play-pcm(list<u8>, u32)`. Host bindings in `nema_host_impl.cpp` call
`AudioService::output(0)`. Gated on `caps::AudioOutput`, so the same JS works on
hardware and simulator.

**3. Binary IDL input params.** `play-pcm` is the first IDL function taking binary
input. Rather than base64-in-a-string (lossy DX, 33% overhead), we taught the
QuickJS codegen (`packages/idl/src/emit/quickjs.ts`) to unmarshal `list<u8>` from
an `ArrayBuffer`/`TypedArray` via a generated `jsToBytes` helper
(`JS_GetTypedArrayBuffer` → `JS_GetArrayBuffer`). `host_cpp` already mapped
`list<u8>` → `const std::vector<uint8_t>&`; `dts` emits `Uint8Array | ArrayBuffer
| number[]`. This is additive — no existing IDL function takes a list input, so
nothing else is affected.

## Consequences

- The simulator test beep is audible in Forge, and any future firmware audio
  (melodies, WAV, streamed PCM) plays unchanged — the firmware owns the waveform,
  the browser just reproduces it. To change what you hear, change the firmware.
- Custom JS apps can play audio: `nema.media["audio-output"].playTone/playPcm/
  setVolume`. The device path (`I2sSpeaker::writePcm`) is parity code following
  the proven `beepTask` pattern; it remains blocked by the same hardware speaker
  question until a transducer is confirmed on `SP1`.
- **`playPcm` is JS-apps-only.** The WASM-C app ABI (`generated/sdk/nema.h`) still
  treats `list` inputs as opaque `int32` handles ("unsupported in v1"), so a WASM
  app calling `play-pcm` gets a handle stub, not bytes. `playTone`/`setVolume` are
  scalar and work for both ABIs. Full binary support for WASM apps is deferred.
- **Load-bearing:** `Module.nemaAudioPcm` must stay wired in
  `VirtualCableTransport` (alongside `nemaPlpOut`) or simulator audio goes silent.
  `MAIN_THREAD_EM_ASM` must be used (not `EM_ASM`) — the beep fires from the GUI
  pthread and the call must be proxied to the page thread with the heap slice
  taken synchronously before the device buffer frees.
- The device I2S clock is fixed at 16 kHz, so `play-pcm`'s `sample-rate` is
  advisory on hardware (honoured in the simulator via Web Audio resampling).
- Ruled out: base64 PCM transport (kept the API binary/raw); re-synthesis in the
  browser (not dynamic); extending the WASM-C binary ABI now (deferred).
