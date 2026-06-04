# Plan 32 — Media Hardware: Mic, Speaker, Camera

Adds microphone input (ES7243E ADC), speaker output (stub for future hardware), and
camera (GC2145 DVP) to the Kairo firmware. Delivers:

- Core HAL interfaces (`IAudioInput`, `IAudioOutput`, `ICamera`)
- `AudioService` + `CameraService` manager objects wired into Runtime
- SkyRizz E32 board drivers for ES7243E mic and GC2145 camera
- **Sounds Settings** screen: hardware list + live level bars + test beep
- **Camera Settings** screen: hardware list with resolution
- **Camera App**: live grayscale viewfinder (1-bit dither, plugin-based)

Hardware reference: Rust reference firmware (`main.rs` + `camera.rs`).

---

## Phase 0 — Documentation Discovery (DONE)

Gathered from codebase. Allowed API summary:

### Core patterns
| Artifact | File | Key facts |
|---|---|---|
| `IService` | `core/include/kairo/service.h` | `name()`, `start()`, `stop()`, `tick(uint64_t nowMs)` |
| `IDriver` | `core/include/kairo/hal/driver.h` | `name()`, `kind()`, `onRegister(Runtime&)`; `DriverKind` enum |
| `IDisplayDriver` | `core/include/kairo/hal/display.h` | Pattern for new HAL interfaces |
| `Runtime` accessors | `core/include/kairo/runtime.h` | Add `audio()`, `camera()` as value members |
| `ServiceContainer` | `core/include/kairo/service/service_container.h` | `registerService<T>()`, `registerAs<I,T>()`, `resolve<I>()` |
| `IScreen` | `core/include/kairo/ui/screen.h` | `enter()`, `update(Key)`, `draw(Canvas&)`, `tick(uint64_t)` |
| `IPlugin` | `core/include/kairo/plugin/plugin.h` | `id()`, `name()`, `version()`, `onLoad()`, `onSelect()` |
| `PluginContext` | `core/include/kairo/plugin/plugin_context.h` | `runtime()`, `pushScreen()`, `log()` |
| `ClockPlugin` | `core/include/kairo/plugins/clock_plugin.h` | Pattern: plugin owns app + `unique_ptr<AppHost> host_` |
| `SettingsScreen` | `core/include/kairo/screens/settings_screen.h` | Dynamic `items_` vector with capability checks |
| `SleepSettingsScreen` | `core/include/kairo/screens/sleep_settings_screen.h` | Pattern: `cursor_`, `invertRect` highlight, footer hint |

### SkyRizz E32 patterns
| Artifact | File | Key facts |
|---|---|---|
| `board_config.h` | `boards/skyrizz-e32/include/.../board_config.h` | `P0_CAM_RST` already defined (bit 2); pins GPIO 0–48 |
| `skyrizz_e32.h` | `boards/skyrizz-e32/include/.../skyrizz_e32.h` | Members: `Xl9535 expander_; LcdDriver lcd_; E32KeyMap keyMap_; Ft6336Touch touch_` |
| `skyrizz_e32.cpp` | `boards/skyrizz-e32/src/skyrizz_e32.cpp` | `driver_.init(rt, expander_)` then `rt.container().registerService/registerAs/hardware/capabilities` |
| `Xl9535` | `boards/skyrizz-e32/include/.../xl9535.h` | `setCamReset(bool asserted)` already exists |
| `CMakeLists` | `boards/skyrizz-e32/CMakeLists.txt` | REQUIRES includes `driver`; add `i2s` + `esp32-camera` |

### Hardware (from Rust reference)
| Device | Interface | Key pins | I2C addr |
|---|---|---|---|
| ES7243E mic ADC | I2S RX | MCLK=3, BCLK=0, WS=38, DIN=39 | 0x11 |
| GC2145 camera | DVP (LCD_CAM) | XCLK=7, PCLK=17, VSYNC=4, HREF=5, D[7:0]=8,10,11,9,18,16,15,6 | 0x3C |
| Camera reset | XL9535 P02 | `P0_CAM_RST` (already in board_config.h) | — |

---

## Phase 1 — Core HAL Interfaces

**Files to create:**
- `firmware/core/include/kairo/hal/audio_input.h`
- `firmware/core/include/kairo/hal/audio_output.h`
- `firmware/core/include/kairo/hal/camera.h`

### `audio_input.h`

```cpp
#pragma once
#include "kairo/hal/driver.h"

namespace kairo {

// IAudioInput — microphone / line-in abstraction.
// Drivers update peakLevel() in their tick() (IService).
struct IAudioInput {
    virtual ~IAudioInput() = default;
    virtual const char* label()       const = 0;  // "Built-in Mic", "Line In"
    virtual float       peakLevel()   const = 0;  // 0.0..1.0, RMS peak, updated per tick
    virtual void        startCapture()      = 0;
    virtual void        stopCapture()       = 0;
};

} // namespace kairo
```

### `audio_output.h`

```cpp
#pragma once
#include "kairo/hal/driver.h"

namespace kairo {

struct IAudioOutput {
    virtual ~IAudioOutput() = default;
    virtual const char* label()                              const = 0;
    virtual float       peakLevel()                          const = 0;  // 0.0..1.0 playback level
    virtual void        setVolume(float v)                         = 0;  // 0.0..1.0
    virtual void        playTone(uint16_t freqHz, uint16_t ms)     = 0;  // test beep
};

} // namespace kairo
```

### `camera.h`

```cpp
#pragma once
#include <cstdint>

namespace kairo {

// ICamera — camera abstraction. Drivers implement IService + ICamera.
// captureFrame() returns a pointer to an internal RGB565 buffer (big-endian,
// width*height*2 bytes) or nullptr if no frame is ready.
// The pointer is valid until the next captureFrame() call.
struct ICamera {
    virtual ~ICamera() = default;
    virtual const char*    label()        const = 0;  // "Front Camera"
    virtual uint16_t       frameWidth()   const = 0;
    virtual uint16_t       frameHeight()  const = 0;
    virtual bool           isOpen()       const = 0;
    virtual bool           open()               = 0;  // init sensor, start DMA
    virtual void           close()              = 0;
    virtual const uint8_t* captureFrame()       = 0;  // blocks until next frame
};

} // namespace kairo
```

**Verification:**
```bash
grep -r "IAudioInput\|IAudioOutput\|ICamera" firmware/core/include/kairo/hal/
# → 3 new header files found
```

---

## Phase 2 — AudioService and CameraService

**Files to create:**
- `firmware/core/include/kairo/services/audio_service.h`
- `firmware/core/src/services/audio_service.cpp`
- `firmware/core/include/kairo/services/camera_service.h`
- `firmware/core/src/services/camera_service.cpp`

**Files to modify:**
- `firmware/core/include/kairo/runtime.h` — add `audio()`, `camera()` accessors + members
- `firmware/core/src/runtime.cpp` — add includes
- `firmware/core/CMakeLists.txt` — add new source files

### `audio_service.h`

AudioService is a plain manager (not IService — drivers handle their own lifecycle).
Holds up to 8 inputs and 8 outputs.

```cpp
#pragma once
#include "kairo/hal/audio_input.h"
#include "kairo/hal/audio_output.h"
#include <cstdint>

namespace kairo {

class AudioService {
public:
    static constexpr int kMaxDevices = 8;

    void addInput (IAudioInput*,  const char* id, const char* desc);
    void addOutput(IAudioOutput*, const char* id, const char* desc);

    int           inputCount()  const { return inputCount_; }
    int           outputCount() const { return outputCount_; }
    IAudioInput*  input (int i) const;  // nullptr if out of range
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
```

### `camera_service.h`

```cpp
#pragma once
#include "kairo/hal/camera.h"
#include <cstdint>

namespace kairo {

class CameraService {
public:
    static constexpr int kMaxDevices = 4;

    void     add(ICamera*, const char* id, const char* desc);
    int      count()       const { return count_; }
    ICamera* get(int i)    const;
    const char* id  (int i) const;
    const char* desc(int i) const;

private:
    struct Entry { ICamera* drv; const char* id; const char* desc; };
    Entry entries_[kMaxDevices] = {};
    int   count_ = 0;
};

} // namespace kairo
```

### Runtime integration

In `runtime.h` — add includes and accessors:
```cpp
// Add to includes:
#include "kairo/services/audio_service.h"
#include "kairo/services/camera_service.h"

// Add to public accessors:
AudioService&  audio();
CameraService& camera();

// Add to private members (value, not unique_ptr — no heap needed):
AudioService   audioService_;
CameraService  cameraService_;
```

In `runtime.cpp`:
```cpp
AudioService&  Runtime::audio()  { return audioService_; }
CameraService& Runtime::camera() { return cameraService_; }
```

In `firmware/core/CMakeLists.txt` — add to SRCS:
```cmake
"src/services/audio_service.cpp"
"src/services/camera_service.cpp"
```

**Verification:**
```bash
grep -n "audio\(\)\|camera\(\)" firmware/core/include/kairo/runtime.h
# → 2 new accessors found
```

---

## Phase 3 — SkyRizz E32 Drivers

### 3a — `board_config.h` additions

Append to `firmware/boards/skyrizz-e32/include/kairo/skyrizze32/board_config.h`:

```cpp
// ── I2S Audio (ES7243E ADC, FPC2) ─────────────────────────────────────────
constexpr int PIN_I2S_MCLK = 3;    // Master clock out → ES7243E
constexpr int PIN_I2S_BCLK = 0;    // Bit clock
constexpr int PIN_I2S_WS   = 38;   // Word select (LRCK)
constexpr int PIN_I2S_DIN  = 39;   // Data in (mic → ESP)

// ── DVP Camera (GC2145, FPC3) ─────────────────────────────────────────────
constexpr int PIN_CAM_XCLK  = 7;   // Master clock out → GC2145 (20 MHz)
constexpr int PIN_CAM_PCLK  = 17;  // Pixel clock in ← GC2145
constexpr int PIN_CAM_VSYNC = 4;   // VSYNC in
constexpr int PIN_CAM_HREF  = 5;   // HREF / DE in (VhdeMode::De on ESP32-S3)
constexpr int PIN_CAM_D0    = 8;
constexpr int PIN_CAM_D1    = 10;
constexpr int PIN_CAM_D2    = 11;
constexpr int PIN_CAM_D3    = 9;
constexpr int PIN_CAM_D4    = 18;
constexpr int PIN_CAM_D5    = 16;
constexpr int PIN_CAM_D6    = 15;
constexpr int PIN_CAM_D7    = 6;
// Camera reset: P0_CAM_RST (XL9535 P02) — already defined above.

// ── I2C device addresses (media) ──────────────────────────────────────────
constexpr uint8_t I2C_ADDR_ES7243E = 0x11;   // Audio ADC
constexpr uint8_t I2C_ADDR_GC2145  = 0x3C;   // Camera SCCB
```

### 3b — `Es7243eMic` driver

**Files:** `include/kairo/skyrizze32/es7243e_mic.h`, `src/es7243e_mic.cpp`

```cpp
// es7243e_mic.h
class Es7243eMic : public IAudioInput, public IService {
public:
    void init(Runtime& rt, Xl9535& expander);

    // IAudioInput
    const char* label()       const override { return "Built-in Mic (ES7243E)"; }
    float       peakLevel()   const override { return peak_; }  // cached, updated in tick()
    void        startCapture()       override;
    void        stopCapture()        override;

    // IService
    const char* name() const override { return "Es7243eMic"; }
    void start() override;
    void stop()  override;
    void tick(uint64_t nowMs) override;  // reads DMA buffer, updates peak_

private:
    bool i2cInit();   // sends ES7243E init sequence from Rust reference
    void i2sInit();   // configures ESP-IDF I2S RX channel

    Runtime*  rt_       = nullptr;
    Xl9535*   expander_ = nullptr;
    void*     i2sHandle_ = nullptr;   // i2s_chan_handle_t (opaque)
    float     peak_      = 0.0f;
    bool      capturing_ = false;
};
```

**ES7243E init sequence** — copy verbatim from Rust reference `es7243_init()`:
```cpp
// 37 (reg, val) pairs applied in order via i2c.write(I2C_ADDR_ES7243E, {reg, val})
// See Rust main.rs es7243_init() for the exact table.
```

**I2S RX config** — ESP-IDF `i2s_new_channel` + `i2s_channel_init_std_mode`:
- 16 kHz sample rate, 32-bit data, 2 channels (stereo)
- MCLK on GPIO3, BCLK on GPIO0, WS on GPIO38, DIN on GPIO39

**`tick()`** — every GUI frame (~15ms):
1. `i2s_channel_read(handle, buf, 1024 * sizeof(int32_t), &bytes_read, 0)` (non-blocking)
2. Peak-detect: iterate 32-bit samples, abs, max → normalize to 0.0..1.0
3. Cache in `peak_`

### 3c — `I2sSpeaker` stub driver

**Files:** `include/kairo/skyrizze32/i2s_speaker.h`, `src/i2s_speaker.cpp`

```cpp
class I2sSpeaker : public IAudioOutput, public IService {
public:
    const char* label()     const override { return "Speaker (stub)"; }
    float       peakLevel() const override { return 0.0f; }
    void        setVolume(float)           override {}
    void        playTone(uint16_t hz, uint16_t ms) override;  // logs, no audio yet

    const char* name() const override { return "I2sSpeaker"; }
    void start() override { if (rt_) rt_->log().info("I2sSpeaker", "stub — no I2S DAC wired"); }
    void stop()  override {}
    void tick(uint64_t) override {}

    void init(Runtime& rt) { rt_ = &rt; }
private:
    Runtime* rt_ = nullptr;
};
```

### 3d — `Gc2145Camera` driver

**Files:** `include/kairo/skyrizze32/gc2145_camera.h`, `src/gc2145_camera.cpp`

> **Dependency note:** Uses `esp_driver_cam` — **built into IDF 5.5.4**, no `idf_component.yml`
> or `idf.py add-dependency` needed. Headers: `esp_cam_ctlr.h`, `esp_cam_ctlr_dvp.h`,
> `esp_cam_ctlr_types.h`.

```cpp
#include <esp_cam_ctlr.h>
#include <esp_cam_ctlr_dvp.h>

class Gc2145Camera : public ICamera, public IService {
public:
    void init(Runtime& rt, Xl9535& expander);

    // ICamera
    const char* label()       const override { return "GC2145 2MP (FPC3)"; }
    uint16_t    frameWidth()  const override { return 320; }
    uint16_t    frameHeight() const override { return 240; }
    bool        isOpen()      const override { return open_; }
    bool        open()               override;  // lazy SCCB init + start controller
    void        close()              override;
    const uint8_t* captureFrame()    override;  // blocks ~1 frame; returns internal buf

    // IService
    const char* name() const override { return "Gc2145Camera"; }
    void start() override;   // allocate DMA buf, pulse P0_CAM_RST, create DVP controller
    void stop()  override;   // esp_cam_ctlr_disable + del_dvp_ctlr + free buf
    void tick(uint64_t) override {}

private:
    bool sccbInit();  // GC2145_DEFAULT_INIT + GC2145_RGB565_OVERRIDE via Wire I2C

    static constexpr uint16_t FRAME_W     = 320;
    static constexpr uint16_t FRAME_H     = 240;
    static constexpr size_t   FRAME_BYTES = FRAME_W * FRAME_H * 2;  // RGB565

    Runtime*               rt_         = nullptr;
    Xl9535*                expander_   = nullptr;
    bool                   open_       = false;
    bool                   sensorInited_ = false;
    esp_cam_ctlr_handle_t  camHandle_  = nullptr;
    uint8_t*               frameBuf_   = nullptr;  // PSRAM DMA buffer
};
```

**GC2145 init tables** — copy verbatim from Rust `camera.rs`:
- `GC2145_DEFAULT_INIT[]` (~200 `{reg, val}` pairs, `{0xFD, delayMs}` for delays)
- `GC2145_RGB565_OVERRIDE[]` (3 pairs: `{0xFE,0x00}`, `{0x84,0x06}`, `{0x86,0x02}`)

Applied via Wire I2C: `Wire.beginTransmission(I2C_ADDR_GC2145); Wire.write(reg); Wire.write(val); Wire.endTransmission()`

**`start()`** — IDF 5.x DVP controller setup:
```cpp
// 1. Allocate frame buffer in PSRAM (DMA-capable)
frameBuf_ = (uint8_t*)heap_caps_malloc(FRAME_BYTES,
                MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

// 2. Pin config from board_config.h
esp_cam_ctlr_dvp_pin_config_t pins = {
    .data_width = CAM_CTLR_DATA_WIDTH_8,
    .data_io    = {PIN_CAM_D0, PIN_CAM_D1, PIN_CAM_D2, PIN_CAM_D3,
                   PIN_CAM_D4, PIN_CAM_D5, PIN_CAM_D6, PIN_CAM_D7},
    .vsync_io   = PIN_CAM_VSYNC,
    .de_io      = PIN_CAM_HREF,   // DE mode, not HSYNC
    .pclk_io    = PIN_CAM_PCLK,
    .xclk_io    = PIN_CAM_XCLK,
};

// 3. DVP config
esp_cam_ctlr_dvp_config_t cfg = {
    .ctlr_id              = 0,
    .clk_src              = CAM_CLK_SRC_DEFAULT,
    .h_res                = FRAME_W,
    .v_res                = FRAME_H,
    .input_data_color_type = CAM_CTLR_COLOR_RGB565,
    .cam_data_width       = 8,
    .xclk_freq            = 20 * 1000 * 1000,  // 20 MHz — same as Rust reference
    .pin                  = &pins,
};
esp_cam_new_dvp_ctlr(&cfg, &camHandle_);

// 4. Camera reset sequence (XCLK must be live before SCCB will ACK)
expander_->setCamReset(true);   // assert reset
vTaskDelay(pdMS_TO_TICKS(50));
expander_->setCamReset(false);  // release reset
vTaskDelay(pdMS_TO_TICKS(300)); // GC2145 boot time
```

**`open()`** — lazy sensor init:
```cpp
if (!sensorInited_) {
    if (!sccbInit()) { log error; return false; }
    sensorInited_ = true;
}
esp_cam_ctlr_enable(camHandle_);
esp_cam_ctlr_start(camHandle_);
open_ = true;
return true;
```

**`captureFrame()`**:
```cpp
esp_cam_ctlr_trans_t trans = {
    .buffer        = frameBuf_,
    .buflen        = FRAME_BYTES,
    .received_size = 0,
};
esp_cam_ctlr_receive(camHandle_, &trans, portMAX_DELAY);
return frameBuf_;
```

**`close()`**:
```cpp
esp_cam_ctlr_stop(camHandle_);
esp_cam_ctlr_disable(camHandle_);
open_ = false;
```

### 3e — Register in `skyrizz_e32.cpp` and `skyrizz_e32.h`

**Add to `skyrizz_e32.h` private members:**
```cpp
Es7243eMic      mic_;
I2sSpeaker      speaker_;
Gc2145Camera    camera_;
```

**Add to `skyrizz_e32.cpp` `describeHardware()`:**
```cpp
// Audio input — ES7243E mic ADC
mic_.init(rt, expander_);
rt.container().registerService(&mic_);
rt.audio().addInput(&mic_, "mic0", "I2S PDM Built-in");
rt.hardware().add({"audio.input", DriverKind::Other, "ES7243E @0x11"});
rt.capabilities().add("audio.input");

// Audio output — stub for future I2S DAC
speaker_.init(rt);
rt.container().registerService(&speaker_);
rt.audio().addOutput(&speaker_, "spk0", "I2S DAC (stub)");
rt.hardware().add({"audio.output", DriverKind::Other, "I2S DAC (stub)"});
rt.capabilities().add("audio.output");

// Camera — GC2145 DVP
camera_.init(rt, expander_);
rt.container().registerService(&camera_);
rt.camera().add(&camera_, "cam0", "GC2145 2MP DVP");
rt.hardware().add({"camera", DriverKind::Other, "GC2145 2MP @0x3C"});
rt.capabilities().add("camera");
```

**Update `CMakeLists.txt`:**
```cmake
SRCS
    "src/skyrizz_e32.cpp"
    "src/xl9535.cpp"
    "src/lcd_driver.cpp"
    "src/e32_key_map.cpp"
    "src/ft6336_touch.cpp"
    "src/es7243e_mic.cpp"
    "src/i2s_speaker.cpp"
    "src/gc2145_camera.cpp"
REQUIRES
    core
    esp32
    espressif__arduino-esp32
    driver
    esp_driver_cam      # ← add (built-in IDF 5.5.4, no add-dependency needed)
    esp_driver_i2s      # ← add (built-in IDF 5.5.4, no add-dependency needed)
```

**Anti-patterns:**
- Do NOT use `i2s_driver_install` (deprecated). Use `i2s_new_channel` / `i2s_channel_init_std_mode`.
- Do NOT use `esp_camera_fb_get` without `esp_camera_fb_return` on previous frame.
- Do NOT hardcode GPIO numbers in driver source — always use `board_config.h` constants.

**Verification:**
```bash
grep -n "audio\.input\|audio\.output\|camera" \
    firmware/boards/skyrizz-e32/src/skyrizz_e32.cpp
# → 3 capability declarations found

grep -n "Es7243eMic\|I2sSpeaker\|Gc2145Camera" \
    firmware/boards/skyrizz-e32/include/kairo/skyrizze32/skyrizz_e32.h
# → 3 member declarations found
```

---

## Phase 4 — Settings Screens

### 4a — `SoundsSettingsScreen`

**Files:**
- `firmware/core/include/kairo/screens/sounds_settings_screen.h`
- `firmware/core/src/screens/sounds_settings_screen.cpp`

**Layout (canvas 240×320):**
```
[STATUS BAR]
─────────────────────
 SOUNDS
─────────────────────
 INPUT
   Built-in Mic (ES)   ████░░░░   0%
   [next input if any]

 OUTPUT
   Speaker (stub)       ░░░░░░░░   0%
   [Test ▶]

─────────────────────
[footer: Back hint]
```

**Class:**
```cpp
class SoundsSettingsScreen : public IScreen {
public:
    explicit SoundsSettingsScreen(Runtime& rt);
    void enter()         override;
    void tick(uint64_t nowMs) override;  // requestRedraw for live level bars
    void update(Key key) override;
    void draw(Canvas& c) override;

private:
    Runtime& rt_;
    int      cursor_ = 0;  // row index (0..N-1 covering all inputs+outputs+test)

    void drawLevelBar(Canvas& c, uint16_t x, uint16_t y, uint16_t w, float level) const;
    // level bar: fillRect(x, y, w*level, CHAR_H, true) for filled portion
    //            fillRect(x+w*level, y, w*(1-level), CHAR_H, false) for remainder
};
```

**`tick()`**: calls `rt_.view().requestRedraw()` every frame so level bars animate.

**`draw()`** structure (follow `SleepSettingsScreen::draw()` as template):
1. `ui::drawTitle(c, "SOUNDS")`
2. Section header "INPUT" in normal text
3. For each `rt_.audio().input(i)`: draw label + level bar
4. Section header "OUTPUT"
5. For each `rt_.audio().output(i)`: draw label + level bar
6. If output selected: draw `[Test ▶]` row (highlighted if cursor on it)
7. Footer: `rt_.input().hintFor(Action::Back)` + " back"

**Level bar rendering** (monochrome canvas):
```cpp
void SoundsSettingsScreen::drawLevelBar(Canvas& c, uint16_t x, uint16_t y,
                                         uint16_t w, float level) const {
    uint16_t fill = (uint16_t)(w * level);
    if (fill > 0) c.fillRect(x,        y, fill,     ui::CHAR_H, true);
    if (fill < w) c.fillRect(x + fill, y, w - fill, ui::CHAR_H, false);
}
```

**Test beep** — when cursor on `[Test ▶]` and Activate pressed:
```cpp
if (rt_.audio().outputCount() > 0)
    rt_.audio().output(0)->playTone(440, 300);
```

### 4b — `CameraSettingsScreen`

**Files:**
- `firmware/core/include/kairo/screens/camera_settings_screen.h`
- `firmware/core/src/screens/camera_settings_screen.cpp`

**Layout:**
```
[STATUS BAR]
─────────────────────
 CAMERA
─────────────────────
  cam0  GC2145 2MP DVP   320×240
  [next camera if any]

─────────────────────
[footer]
```

**Class:**
```cpp
class CameraSettingsScreen : public IScreen {
public:
    explicit CameraSettingsScreen(Runtime& rt);
    void enter()         override;
    void update(Key key) override;
    void draw(Canvas& c) override;
private:
    Runtime& rt_;
    int      cursor_ = 0;
};
```

**`draw()`**:
1. `ui::drawTitle(c, "CAMERA")`
2. For each `rt_.camera().get(i)`: row with `id + " " + desc + " " + WxH`
3. If `rt_.camera().count() == 0`: draw "No camera hardware"
4. Footer: Back hint

### 4c — Wire into `SettingsScreen`

**Modify `firmware/core/include/kairo/screens/settings_screen.h`:**

Add private members:
```cpp
SoundsSettingsScreen  sounds_;
CameraSettingsScreen  cameraSettings_;
```

Add includes:
```cpp
#include "kairo/screens/sounds_settings_screen.h"
#include "kairo/screens/camera_settings_screen.h"
```

**Modify `settings_screen.cpp` `buildMenu()`:**
```cpp
if (rt_.capabilities().has("audio.input") ||
    rt_.capabilities().has("audio.output"))
    items_.push_back({"Sounds"});

if (rt_.capabilities().has("camera"))
    items_.push_back({"Camera"});
```

**Modify `handleSelect()`:**
```cpp
} else if (std::strcmp(label, "Sounds") == 0) {
    rt_.view().push(sounds_);
} else if (std::strcmp(label, "Camera") == 0) {
    rt_.view().push(cameraSettings_);
}
```

**Update `firmware/core/CMakeLists.txt` SRCS:**
```cmake
"src/screens/sounds_settings_screen.cpp"
"src/screens/camera_settings_screen.cpp"
```

**Verification:**
```bash
grep -n "Sounds\|Camera\|SoundsSettings\|CameraSettings" \
    firmware/core/src/screens/settings_screen.cpp
# → capability checks + screen pushes found
```

---

## Phase 5 — Camera App and Plugin

### 5a — `CameraApp`

**Files:**
- `firmware/core/include/kairo/apps/camera_app.h`
- `firmware/core/src/apps/camera_app.cpp`

Uses `IScreen` (not `ComponentApp`) because it needs `drawBitmap` for raw frame data.

```cpp
class CameraApp : public IScreen {
public:
    explicit CameraApp(Runtime& rt);

    ScreenMode mode() const override { return ScreenMode::Fullscreen; }

    void enter() override;
    void tick(uint64_t nowMs) override;
    void update(Key key) override;
    void draw(Canvas& c) override;

private:
    Runtime&  rt_;
    ICamera*  cam_     = nullptr;   // resolved in enter()
    uint64_t  lastFrameMs_ = 0;
    uint8_t   fps_     = 0;
    uint8_t   frameCount_ = 0;
    uint64_t  fpsWindowMs_ = 0;

    // 1-bit dither buffer: width*height bits / 8 bytes
    // Canvas is 240×320 logical; frame is 320×240 → rotate 90° + threshold
    static constexpr uint16_t DITHER_W = 240;
    static constexpr uint16_t DITHER_H = 240;  // square crop of 320×240 frame
    static constexpr size_t   DITHER_BYTES = (DITHER_W * DITHER_H + 7) / 8;
    uint8_t ditherBuf_[DITHER_BYTES] = {};

    void buildDitherBuf(const uint8_t* rgb565, uint16_t fw, uint16_t fh);
};
```

**`enter()`**:
```cpp
void CameraApp::enter() {
    cam_ = nullptr;
    if (rt_.camera().count() > 0)
        cam_ = rt_.camera().get(0);
    if (cam_ && !cam_->isOpen())
        cam_->open();
    rt_.view().requestRedraw();
}
```

**`tick()`**: calls `rt_.view().requestRedraw()` to trigger continuous frames.

**`draw()`**:
1. `c.clear()` (full black — Fullscreen mode, no status bar)
2. If `!cam_`: draw "No camera" message, Back hint
3. Call `cam_->captureFrame()` → `const uint8_t* rgb` (RGB565, 320×240)
4. `buildDitherBuf(rgb, 320, 240)` → fills `ditherBuf_`
5. `c.drawBitmap(0, (c.height()-DITHER_H)/2, DITHER_W, DITHER_H, ditherBuf_)`
6. Draw FPS: `c.drawText(c.width()-24, 2, fpsStr, true)`
7. Draw Back hint: `c.drawText(4, c.height()-ui::CHAR_H-2, hintStr, true)`

**`buildDitherBuf()`** — luminance threshold (center-crop 240×240 from 320×240):
```cpp
void CameraApp::buildDitherBuf(const uint8_t* rgb, uint16_t fw, uint16_t fh) {
    memset(ditherBuf_, 0, DITHER_BYTES);
    uint16_t cropX = (fw - DITHER_W) / 2;  // = 40
    for (uint16_t row = 0; row < DITHER_H && row < fh; row++) {
        for (uint16_t col = 0; col < DITHER_W; col++) {
            size_t idx = ((size_t)row * fw + cropX + col) * 2;
            uint16_t px = ((uint16_t)rgb[idx] << 8) | rgb[idx + 1];
            uint8_t  r  = (px >> 11) & 0x1F;
            uint8_t  g  = (px >>  5) & 0x3F;
            uint8_t  b  =  px        & 0x1F;
            // Luminance approx: scale to comparable range, threshold at ~50%
            uint32_t lum = (uint32_t)r * 9 + (uint32_t)g * 9 + (uint32_t)b * 4;
            if (lum > 140) {  // ~50% of max (31*9 + 63*9 + 31*4 = 863; 140/863 ≈ 16%)
                size_t bitIdx = (size_t)row * DITHER_W + col;
                ditherBuf_[bitIdx / 8] |= (uint8_t)(0x80 >> (bitIdx % 8));
            }
        }
    }
}
```

**`update(Key key)`**: `Key::Cancel` → `rt_.view().pop()` + `cam_->close()`.

### 5b — `CameraPlugin`

**Files:**
- `firmware/core/include/kairo/plugins/camera_plugin.h`
- `firmware/core/src/plugins/camera_plugin.cpp`

Copy `ClockPlugin` pattern exactly:

```cpp
class CameraPlugin : public IPlugin {
public:
    PluginId    id()      const override { return "com.kairo.camera"; }
    const char* name()    const override { return "Camera"; }
    const char* version() const override { return "1.0.0"; }

    CameraPlugin();
    ~CameraPlugin() override;

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    CameraApp                app_;  // owns the app instance
    std::unique_ptr<AppHost> host_;
};
```

```cpp
// camera_plugin.cpp
void CameraPlugin::onLoad(PluginContext& ctx) {
    if (!ctx.capabilities().has("camera"))
        ctx.log().warn("CameraPlugin", "no camera capability — app will show error");
}
void CameraPlugin::onSelect(PluginContext& ctx) {
    host_ = std::make_unique<AppHost>(ctx.runtime(), app_);
    ctx.pushScreen(*host_);
}
```

### 5c — Register in targets

In `firmware/targets/skyrizz-e32/main/main.cpp`, add:
```cpp
#include "kairo/plugins/camera_plugin.h"
// ...
kairo::CameraPlugin cameraPlugin;
// in setup(), after rt.start():
rt.plugins().load(cameraPlugin);
```

**Update `firmware/core/CMakeLists.txt` SRCS:**
```cmake
"src/apps/camera_app.cpp"
"src/plugins/camera_plugin.cpp"
```

**Verification:**
```bash
grep -n "CameraPlugin\|cameraPlugin" \
    firmware/targets/skyrizz-e32/main/main.cpp
# → plugin declared and loaded

grep -n "com.kairo.camera" \
    firmware/core/src/plugins/camera_plugin.cpp
# → plugin ID found
```

---

## Phase 6 — Final Verification

```bash
# 1. All new HAL files exist
ls firmware/core/include/kairo/hal/audio_input.h \
   firmware/core/include/kairo/hal/audio_output.h \
   firmware/core/include/kairo/hal/camera.h

# 2. Services registered in Runtime
grep "audioService_\|cameraService_" firmware/core/include/kairo/runtime.h
grep "audio()\|camera()" firmware/core/include/kairo/runtime.h

# 3. Board drivers present
ls firmware/boards/skyrizz-e32/src/es7243e_mic.cpp \
   firmware/boards/skyrizz-e32/src/i2s_speaker.cpp \
   firmware/boards/skyrizz-e32/src/gc2145_camera.cpp

# 4. Capabilities declared
grep "audio.input\|audio.output\|camera" \
    firmware/boards/skyrizz-e32/src/skyrizz_e32.cpp

# 5. Settings screens wired
grep "Sounds\|Camera" firmware/core/src/screens/settings_screen.cpp

# 6. Camera plugin registered
grep "cameraPlugin" firmware/targets/skyrizz-e32/main/main.cpp

# 7. Build succeeds
bun build:skyrizz-e32
```

**Anti-patterns to guard against:**
- Never branch on `board->name() == "skyrizz-e32"` in core/app code — use `rt.capabilities().has()`
- Never call `rt.audio()` or `rt.camera()` before `rt.registerServices()` completes
- Never block the GUI thread in `captureFrame()` for more than one frame (~30ms budget)
- Never use deprecated I2S API `i2s_driver_install` — use `i2s_new_channel`

---

## File Index

### New files — `firmware/core/`
```
include/kairo/hal/audio_input.h
include/kairo/hal/audio_output.h
include/kairo/hal/camera.h
include/kairo/services/audio_service.h
include/kairo/services/camera_service.h
include/kairo/screens/sounds_settings_screen.h
include/kairo/screens/camera_settings_screen.h
include/kairo/apps/camera_app.h
include/kairo/plugins/camera_plugin.h
src/services/audio_service.cpp
src/services/camera_service.cpp
src/screens/sounds_settings_screen.cpp
src/screens/camera_settings_screen.cpp
src/apps/camera_app.cpp
src/plugins/camera_plugin.cpp
```

### New files — `firmware/boards/skyrizz-e32/`
```
include/kairo/skyrizze32/es7243e_mic.h
include/kairo/skyrizze32/i2s_speaker.h
include/kairo/skyrizze32/gc2145_camera.h
src/es7243e_mic.cpp
src/i2s_speaker.cpp
src/gc2145_camera.cpp
```

### Modified files
```
firmware/core/include/kairo/runtime.h          (+ audio()/camera() accessors + members)
firmware/core/src/runtime.cpp                  (+ accessor impls)
firmware/core/CMakeLists.txt                   (+ 5 new sources)
firmware/boards/skyrizz-e32/include/.../board_config.h   (+ audio/camera pins)
firmware/boards/skyrizz-e32/include/.../skyrizz_e32.h    (+ 3 driver members)
firmware/boards/skyrizz-e32/src/skyrizz_e32.cpp          (+ 3 driver registrations)
firmware/boards/skyrizz-e32/CMakeLists.txt               (+ 3 sources + 2 REQUIRES)
firmware/core/src/screens/settings_screen.cpp            (+ Sounds/Camera items)
firmware/core/include/kairo/screens/settings_screen.h    (+ 2 screen members)
firmware/targets/skyrizz-e32/main/main.cpp               (+ CameraPlugin)
```
