# Simulator audio (raw PCM → Web Audio)

How the simulator produces sound in the browser, so the **Settings → Sounds →
Test Beep** path is audible in Forge (no hardware speaker involved).

## Why / design

The WASM build has no DAC. On the SkyRizz E32 the `IAudioOutput` test beep drives
the NS4168 amplifier over I2S; in the browser there is no such peripheral.

The bridge is **raw**, not an RPC. The firmware does NOT tell the browser "play a
440 Hz tone" and let it re-synthesize — that would only ever work for tones. The
firmware instead synthesizes the **actual PCM samples** the NS4168 would receive
(exactly as the device audio path does) and streams those raw samples to the host
page. Forge is a **dumb DAC**: it plays whatever samples arrive, nothing more.
This stays correct for any future audio — melodies, WAV, streamed PCM — because
the firmware owns the waveform and the browser just reproduces it.

## Path

```
SoundsSettingsScreen (Test Beep)
  → IAudioOutput::playTone(440, 300)
    → WasmSpeaker::playTone               (firmware/platforms/wasm)
        synthesizes 16 kHz mono int16 square-wave PCM (same as the device)
      → MAIN_THREAD_EM_ASM → Module.nemaAudioPcm(Int16Array, sampleRate)
        → VirtualCableTransport wires Module.nemaAudioPcm
          → SimAudio.playPcm()            (packages/forge/src/lib/audio)
            → Web Audio AudioBuffer (raw samples) ← you hear this
```

This mirrors the existing display/PLP bridge (`WasmCableTransport::send` →
`Module.nemaPlpOut`): the host installs named hooks on the emscripten `Module`,
and firmware calls them via `MAIN_THREAD_EM_ASM` (thread-safe + synchronous — the
beep fires from the GUI pthread, and `HEAP16.slice` copies the samples before the
device buffer goes out of scope). `SimAudio` schedules chunks back-to-back on a
shared cursor, so a continuous stream plays seamlessly and one-shot beeps play
immediately.

## Files

| Layer | File | Role |
|---|---|---|
| Firmware HAL | `firmware/platforms/wasm/include/nema/wasm/wasm_speaker.h` / `src/wasm_speaker.cpp` | `IAudioOutput` impl; synthesizes PCM, streams via `Module.nemaAudioPcm` |
| Firmware platform | `firmware/platforms/wasm/src/wasm_platform.cpp` | registers `WasmSpeaker`, adds `caps::AudioOutput` (this is what makes **Sounds** appear) |
| WASM app import | `firmware/core/src/wasm/wasm_audio.cpp` | `audio.*` host imports (`audio_play_tone`/`audio_set_volume`) for WASM-C apps → `rt.audio()` |
| Host page | `packages/forge/src/lib/audio/SimAudio.ts` | raw-PCM player (`playPcm`) — int16 → AudioBuffer, seamless scheduling |
| Host wiring | `packages/forge/src/lib/transport/VirtualCableTransport.ts` | installs `Module.nemaAudioPcm` |

## Notes / limitations

- **Output only.** No simulated mic input yet, so the Sounds screen shows "No
  input devices". The device meter (`peakLevel`) reads 0% — the host synthesizes
  the tone, there is no sample readback.
- **Square wave** PCM (16 kHz mono int16), matching the device NS4168 test beep.
  Amplitude is scaled by the device volume with headroom (square waves are loud).
  The waveform is generated on the firmware side — to change what you hear, change
  the firmware, not the browser.
- **Autoplay policy.** The `AudioContext` is created lazily and `resume()`d on
  each beep. By the time the user reaches Settings → Sounds they have already
  interacted with the page, so playback is allowed.
- Settings → Sounds visibility is gated on `caps::AudioInput || caps::AudioOutput`
  (`settings_screen.cpp`); the simulator now advertises `caps::AudioOutput`.

## Using it from a custom app

The same `IAudioOutput` is exposed to custom apps through the `nema:media` API
(IDL `api/media.pidl`), gated on the `audio.output` capability:

```js
// JS app (QuickJS). Works on hardware AND the simulator — same HAL.
nema.media["audio-output"].setVolume(80);          // 0..100
nema.media["audio-output"].playTone(440, 300);     // freq Hz, duration ms

// Raw PCM — you provide the samples, the device/browser reproduces them exactly.
const rate = 16000, secs = 0.5;
const pcm = new Int16Array(rate * secs);
for (let i = 0; i < pcm.length; i++)
  pcm[i] = Math.sin(2 * Math.PI * 440 * i / rate) * 12000;  // sine, ±12000
nema.media["audio-output"].playPcm(pcm, rate);     // Int16Array | ArrayBuffer | Uint8Array
```

Wiring: `playPcm` → host `audio_output_play_pcm` (`nema_host_impl.cpp`) →
`IAudioOutput::writePcm` → `WasmSpeaker` (stream to Web Audio) or `I2sSpeaker`
(I2S to NS4168). `playPcm`'s binary argument is the first **binary input param**
in the IDL — `quickjs.ts` reads it from an `ArrayBuffer`/`TypedArray` via the
generated `jsToBytes` helper (other emitters: `host_cpp` already maps
`list<u8>` → `const std::vector<uint8_t>&`; the WASM-C app ABI still treats
binary inputs as opaque, so `playPcm` is JS-apps-only for now — `playTone`/
`setVolume` are scalar and work for both).

### WASM-C apps

`playTone`/`setVolume` reach WASM-C apps through the `audio.*` host imports in
`firmware/core/src/wasm/wasm_audio.cpp` (`linkAudioImports`, wired in
`wasm_engine.cpp`), which forward to `rt.audio()` — the same `IAudioOutput` as JS.
The SDK declares them in `nema_api.h`:

```c
#include "nema_api.h"
audio_set_volume(75);              // 0..100
audio_play_tone(440, 300);         // freq Hz, duration ms — non-blocking on the sim
```

Non-blocking on the simulator (the tone is synthesized + streamed, then plays
async), so an app can fire one note per frame and keep animating — see
`examples/hbd` (a looped "Happy Birthday" melody over the cake scene). After
adding/using these, rebuild the WASM firmware (`bun run forge:wasm`) so the
`audio` import table is present.

## Try it

```bash
bun run forge:wasm     # rebuild WASM into Forge, then start the dev server
# open /simulator → Boot → Settings → Sounds → Test Beep 440Hz
```
