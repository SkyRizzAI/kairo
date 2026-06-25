# nema:media/audio-output

> gated: `audio.output`  
> Package: `nema:media@1.0`  
## Functions

| Function | Returns | Flags |
|---|---|---|
| `list() → list<string>` | `list<string>` | — |
| `set-volume(level: u8)` | `void` | — |
| `play-tone(freq: u16, ms: u16)` | `void` | — |
| `play-pcm(data: list<u8>, sample-rate: u32)` | `void` | — |

### `list`

List available audio output devices.

**Returns:** `list<string>`

### `set-volume`

Set output gain, 0..100 (%). Maps to IAudioOutput::setVolume.

**Parameters:**

- `level`: `u8`

### `play-tone`

Play a simple test tone (square wave) on the default output. freq in Hz, duration in ms. Maps to IAudioOutput::playTone.

**Parameters:**

- `freq`: `u16`
- `ms`: `u16`

### `play-pcm`

Play RAW 16-bit mono PCM on the default output. `data` is little-endian int16 samples (pass an Int16Array / ArrayBuffer / Uint8Array of bytes); `sample-rate` in Hz. Maps to IAudioOutput::writePcm — the device/simulator reproduces exactly these samples (no re-synthesis). On hardware the I2S clock is fixed at 16 kHz; the simulator honours sample-rate.

**Parameters:**

- `data`: `list<u8>`
- `sample-rate`: `u32`
