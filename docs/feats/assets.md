# Asset System — Plan 82

> Single source of truth for all assets in Palanu firmware.
> Supersedes the Plan 71 `asset-loader.md` (which described the old `.bm`-seeding approach).

---

## 3-Tier Model

| Tier | Kind | Storage | Load cost | When to use |
|---|---|---|---|---|
| **T1** | Static icons (`kIc*`) | C array → flash | zero | Status bar icons, UI chrome, arrows |
| **T2** | Short animations (`kAnim*`) | C array → flash | zero | Launcher icons (14×14), spinner |
| **T3** | Large animations (`.panim`) | LittleFS / host FS | file I/O at screen-open | Desktop wallpapers, dolphin showcase |

---

## T1 — System Icons

Header: `nema/assets/system_icons.h`  
Sources: `firmware/core/include/nema/assets/icons/`

```cpp
#include "nema/assets/system_icons.h"
// All icons are nema::assets::Icon descriptors:
// struct Icon { uint8_t w; uint8_t h; const uint8_t* data; };

nema::assets::icBattery        // 25×8  — battery level bar (no fill, level drawn by code)
nema::assets::icCharging       // 9×10  — lightning bolt
nema::assets::icBleIdle        // 5×8   — Bluetooth (unpaired)
nema::assets::icBleConnected   // 16×8  — Bluetooth + device icon
nema::assets::icBleBeacon      // 7×8   — Bluetooth broadcasting
nema::assets::icSdMounted      // 11×8  — SD card OK
nema::assets::icSdFail         // 11×8  — SD card error
nema::assets::icLock           // 9×8   — padlock (hidden/locked mode)
nema::assets::icArrowUp        // 5×3   — small nav arrow up
nema::assets::icArrowDown      // 5×3   — small nav arrow down
nema::assets::icArrowLeft      // 3×5   — small nav arrow left
nema::assets::icArrowRight     // 3×5   — small nav arrow right
nema::assets::icWifiOn         // 10×8  — WiFi (TODO: replace placeholder)
nema::assets::icWifiOff        // 10×8  — WiFi off (TODO: replace placeholder)
```

Drawing an icon (1-bit, MSB-first, `bitIdx = row * w + col`):
```cpp
void drawIcon(Canvas& c, uint16_t x, uint16_t y, const nema::assets::Icon& ic) {
    for (uint16_t row = 0; row < ic.h; row++)
        for (uint16_t col = 0; col < ic.w; col++) {
            uint32_t bi = (uint32_t)row * ic.w + col;
            if ((ic.data[bi / 8] >> (7 - (bi % 8))) & 1)
                c.drawPixel(x + col, y + row, true);
        }
}
```

### Source PNGs (Flipper Zero Momentum firmware)

Converted from `refs/flipper-zero-momentum-firmware/assets/`:

| Symbol | Source path | Size |
|---|---|---|
| `icBattery` | `StatusBar/Battery_25x8.png` | 25×8 |
| `icCharging` | `StatusBar/Charging_lightning_9x10.png` | 9×10 |
| `icBleIdle` | `StatusBar/Bluetooth_Idle_5x8.png` | 5×8 |
| `icBleConnected` | `StatusBar/Bluetooth_Connected_16x8.png` | 16×8 |
| `icBleBeacon` | `StatusBar/BLE_beacon_7x8.png` | 7×8 |
| `icSdMounted` | `StatusBar/SDcardMounted_11x8.png` | 11×8 |
| `icSdFail` | `StatusBar/SDcardFail_11x8.png` | 11×8 |
| `icLock` | `StatusBar/Hidden_window_9x8.png` | 9×8 |
| `icArrowUp` | `Interface/SmallArrowUp_3x5.png` | 3×5 |
| `icArrowDown` | `Interface/SmallArrowDown_3x5.png` | 3×5 |
| `icArrowLeft` | `Common/ButtonLeftSmall_3x5.png` | 3×5 |
| `icArrowRight` | `Common/ButtonRightSmall_3x5.png` | 3×5 |
| `icWifiOn`, `icWifiOff` | — (Flipper has no WiFi) | 10×8 hand-coded TODO |

---

## T2 — System Animations

Header: `nema/assets/system_anims.h`  
Sources: `firmware/core/include/nema/assets/anims/`

```cpp
#include "nema/assets/system_anims.h"
// All symbols are nema::anim::Animation references:

nema::assets::animIconSettings   // 14×14, 10 frames — Settings icon
nema::assets::animIconBadusb     // 14×14, 11 frames — BadUSB icon
nema::assets::animIconNfc        // 14×14, 4 frames  — NFC icon (app placeholder)
nema::assets::animIconInfrared   // 14×14, 6 frames  — Infrared icon
nema::assets::animIconApps       // 14×14, 9 frames  — Apps icon
nema::assets::animSpinner        // 8×8,   5 frames  — loading spinner
```

Playing an animation:
```cpp
nema::anim::AnimationPlayer player(nema::assets::animIconSettings);
player.start();
// In GUI-thread tick:
anim::AnimationManager::instance().registerPlayer(player);
// On screen pause:
anim::AnimationManager::instance().unregisterPlayer(player);
player.stop();
```

---

## T3 — `.panim` File Format

Version 1, little-endian.

```
Offset  Size  Field
 0      4     Magic "PANM" (0x50 0x41 0x4E 0x4D)
 4      1     version = 1
 5      2     width  (pixels, uint16 LE)
 7      2     height (pixels, uint16 LE)
 9      1     frameRate (fps)
10      1     passiveCount  — frames in idle loop (playhead 0..passiveCount-1)
11      1     activeCount   — frames in triggered sequence
12      1     uniqueFrameCount — actual distinct bitmaps stored
13      1     framesOrderLen   — 0 = sequential playback
14      N     framesOrder[]  (N = framesOrderLen, uint8 indices into bitmaps)
14+N    M     raw bitmaps    (M = uniqueFrameCount × ceil(width/8) × height bytes)
```

Bit encoding (same as Canvas::drawBitmap):
`bitIdx = row * width + col; bit = (data[bitIdx/8] >> (7-(bitIdx%8))) & 1`

### Available `.panim` files (provisioned in VFS under `anims/`)

| File | Frames | Size | Source |
|---|---|---|---|
| `anims/doom.panim` | 39 unique | ~40 KB | `dolphin/external/L1_Doom_128x64` |
| `anims/boxing.panim` | 7 unique | ~7 KB | `dolphin/external/L1_Boxing_128x64` |
| `anims/sleep.panim` | 4 unique | ~4 KB | `dolphin/external/L1_Sleep_128x64` |
| `anims/tv.panim` | 8 unique | ~6 KB | `dolphin/external/L1_Tv_128x47` |
| `anims/akira.panim` | 36 unique | ~37 KB | `dolphin/external/L1_Akira_128x64` |
| `anims/cry.panim` | 8 unique | ~8 KB | `dolphin/external/L1_Cry_128x64` |
| `anims/read.panim` | 9 unique | ~9 KB | `dolphin/external/L1_Read_128x64` |
| `anims/hacking.panim` | 5 unique | ~5 KB | `dolphin/external/L1_Hacking_128x64` |
| `anims/dj.panim` | 37 unique | ~38 KB | `dolphin/external/L1_DJ_128x64` |

### Loading a `.panim` file

```cpp
#include "nema/ui/asset_loader.h"

nema::asset::PanimAsset asset;
if (asset.load(*rt.fs(), "anims/doom.panim")) {
    auto player = std::make_unique<nema::anim::AnimationPlayer>(asset.animation());
    player->start();
    // tick: player->tick(nowMs)
    // draw: c.drawBitmap(x, y, w, h, player->currentFrameData())
}
// asset must outlive player
```

`PanimAsset` must outlive any `AnimationPlayer` that uses it — the player stores a
reference to `asset.animation()` which points into `asset.rawData`.

---

## AnimationPlayer — Playhead API

`AnimationPlayer` uses `playhead_` (position in the playback sequence) instead of
`frame_` (bitmap index). `resolveFrame()` maps playhead → bitmap index via `framesOrder[]`.

```cpp
player.start();                // start passive loop from playhead 0
player.pause();                // freeze on current frame
player.stop();                 // reset to playhead 0
player.triggerActive();        // jump to passive→active boundary for one active cycle
bool advanced = player.tick(nowMs);   // advance if enough time elapsed; returns true if frame changed
const uint8_t* bits = player.currentFrameData();  // current bitmap
uint8_t w = player.width(), h = player.height();
```

Passive loop: playhead 0 → `passiveCount-1` (wraps).  
Active sequence: on `triggerActive()`, playhead jumps to `passiveCount`, plays to end, then returns to passive loop.

---

## Asset Toolchain

Location: `tools/asset_gen/`

```bash
cd tools/asset_gen
bun install       # install jimp etc.

# Convert single PNG → C array header
bun run src/index.ts png2c \
  --input path/to/icon.png \
  --out firmware/core/include/nema/assets/icons/ic_foo.h \
  --sym kIcFoo

# Convert Flipper animation directory → .panim
bun run src/index.ts seq2panim \
  --input refs/.../L1_Boxing_128x64 \
  --out firmware/assets/anims/boxing.panim

# Batch convert all (reads tools/asset_gen/asset_gen.config.json)
bun run src/index.ts batch --config asset_gen.config.json
```

Config file: `tools/asset_gen/asset_gen.config.json`.

---

## System Apps vs User Apps

`AppManifest.category` determines which launcher section an app appears in:

| Category | Meaning | Examples |
|---|---|---|
| `"Apps"` (default) | User-facing launchable features | DolphinApp, HelloApp, JS apps |
| `"System"` | Core platform tools | BadUSB |

Apps declare their category by overriding `IApp::category()`:
```cpp
const char* category() const override { return "System"; }
```

The launcher Wii grid skin renders a section header between groups.

---

## Files

| Path | Purpose |
|---|---|
| `firmware/core/include/nema/assets/icons/` | T1 icon C arrays (ic_*.h) |
| `firmware/core/include/nema/assets/anims/` | T2 animation frame C arrays (*_raw.h) |
| `firmware/core/include/nema/assets/system_icons.h` | T1 aggregate header |
| `firmware/core/include/nema/assets/system_anims.h` | T2 aggregate header |
| `firmware/core/include/nema/ui/asset_loader.h` | PanimAsset, BitmapAsset, AnimAsset |
| `firmware/core/src/ui/asset_loader.cpp` | PanimAsset::load() implementation |
| `firmware/core/include/nema/ui/dolphin_anim.h` | DOLPHIN_ENTRIES[] path catalog |
| `firmware/core/src/ui/dolphin_anim.cpp` | DOLPHIN_ENTRIES[] definitions |
| `firmware/assets/anims/` | Generated .panim files (committed, provisioned in VFS) |
| `tools/asset_gen/` | Bun/TypeScript toolchain (png2c, seq2panim, batch) |
