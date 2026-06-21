# Plan 82 — Asset Architecture, System Icons & System Apps

## Goals

1. **Asset architecture** — 3-tier model (T1 flash icons, T2 flash anims, T3 FS anims)
2. **`frames_order` support** — extend `Animation` + `AnimationPlayer` for non-linear playback
3. **`.panim` binary format** — spec + `AssetLoader` for T3 filesystem animations
4. **System icons** — port Flipper refs icons to C arrays; wire into status bar
5. **Toolchain** — `tools/asset_gen/` (Bun/TypeScript) for PNG→C and PNG sequence→`.panim`
6. **System apps** — clarify `category = "System"` convention; move BadUSB there
7. **Docs** — `docs/feats/assets.md` as single source of truth for all assets

---

## Background

### Existing state

- `dolphin_showcase.cpp` (874 KB) — 10 Flipper animations compiled as C arrays. **Wrong
  tier**: large multi-frame animations should not live in firmware binary.
- `builtin_animations.cpp` — 4-frame spinner (correct: T2).
- `AnimationPlayer` plays frames **sequentially only** (`frame_++`). Flipper's
  `frames_order` (e.g. `0 1 2 1 3`) is not supported — complex loops flatten to
  worst-case frame count.
- Status bar has no icons (wifi, BLE, battery, SD card) — all drawn as text or missing.
- `AppManifest.category` defaults to `"Apps"`. System-level built-ins (BadUSB, Files,
  Logs, Settings) are not distinguished from user apps in the launcher.

### Terminology

| Term | Definition |
|---|---|
| **System app** | `AppKind::BuiltIn` + `category = "System"` — core features always present (Files, Logs, Settings, BadUSB) |
| **User app** | `AppKind::BuiltIn\|Custom` + `category = "Apps"` — installed demos, JS/.papp bundles |
| **Service** | `AppType::Service` — background daemon, never shown in launcher |

No new enums needed — `category` already exists on `AppManifest`.

---

## Asset Taxonomy

### T1 — System Icons (C array, in-flash)

Small static 1-bit bitmaps. Always available, zero I/O cost. Lives in
`firmware/core/include/nema/assets/icons/` as C headers.

Named as `ic_<name>_<W>x<H>.h`, exposed via `nema/assets/system_icons.h`.

```cpp
// Usage
#include "nema/assets/system_icons.h"
draw::icon(canvas, x, y, nema::assets::kIcBattery, 25, 8);
```

### T2 — System Animations (C array, in-flash)

Short frame sequences for UI feedback or small icon animations. Same location
as T1 but with `Animation` struct. ≤ 30 frames, ≤ ~50 KB total.

```cpp
// Usage
AnimationPlayer player(nema::assets::kAnimSpinner);
player.start();
```

### T3 — User / Large Animations (binary file, LittleFS)

Multi-frame animations (desktop wallpapers, showcase, custom user anims). Stored
as `.panim` files on LittleFS (ESP32) or host FS (simulator).

```cpp
// Usage
auto* anim = rt.assets().loadAnim("/anims/boxing.panim");  // heap, caller owns
```

---

## `.panim` Binary Format

Format version 1. All values little-endian.

```
Offset  Size  Field
0       4     Magic: 0x50 0x41 0x4E 0x4D  ("PANM")
4       1     Version: 1
5       2     Width  (pixels)
7       2     Height (pixels)
9       1     frameRate (fps)
10      1     passiveFrameCount  (frames in idle loop)
11      1     activeFrameCount   (frames in triggered sequence)
12      1     framesOrderLen     (0 = sequential, no order table)
13      N     framesOrder[]      (uint8 indices, N = framesOrderLen)
13+N    M     raw frame bitmaps  (width*height/8 bytes each, MSB first)
              M = uniqueFrameCount * (width*height/8)
              uniqueFrameCount = passiveFrameCount + activeFrameCount
```

`framesOrder` covers the full playback sequence (passive + active combined), matching
Flipper's `Frames order` field. When `framesOrderLen == 0`, frames play 0→N−1.

---

## `frames_order` Extension

### `Animation` struct (`firmware/core/include/nema/ui/animation.h`)

Add fields (backward-compatible — null = sequential):

```cpp
struct Animation {
    const AnimationFrame* frames;
    uint8_t               frameCount;      // unique bitmap count
    uint8_t               frameRate;
    bool                  loop;

    // New — null means sequential playback (existing behaviour preserved)
    const uint8_t*        framesOrder;     // playback index sequence
    uint8_t               framesOrderLen;  // length of framesOrder[]
    uint8_t               passiveCount;    // [0..passiveCount) = idle loop
    uint8_t               activeCount;     // [passiveCount..end) = triggered
};
```

### `AnimationPlayer` (`animation_player.cpp`)

Replace `frame_` (bitmap index) with `playhead_` (position in `framesOrder[]`):

```cpp
// tick() advance — after:
uint8_t resolveFrame() const {
    if (!def_.framesOrder || def_.framesOrderLen == 0)
        return playhead_;
    return def_.framesOrder[playhead_ % def_.framesOrderLen];
}
// currentFrameData() uses resolveFrame() instead of frame_
```

`AnimationPlayer` gains `triggerActive()` — switches playhead to `passiveCount`
offset for one active cycle, then returns to passive loop.

---

## Asset Loader

```
nema/assets/
  system_icons.h     — T1: all icon C-array externs + kIc* constants
  system_anims.h     — T2: all anim externs + kAnim* constants
  asset_store.h      — T3: AssetStore singleton (loadAnim / freeAnim)
  asset_loader.h     — platform interface (IAssetLoader)

platforms/esp32/src/
  esp32_asset_loader.cpp   — IAssetLoader via LittleFS

platforms/simulator/src/
  sim_asset_loader.cpp     — IAssetLoader via fopen() host FS
```

`AssetStore::loadAnim(path)` reads `.panim`, allocates frame data on heap,
returns `Animation*`. Caller frees via `AssetStore::freeAnim(anim)`.

---

## System Icons Catalog

Sources: `refs/flipper-zero-momentum-firmware/assets/`

### T1 Status Bar Icons

| Symbol | Source file | Size | Status |
|---|---|---|---|
| `kIcBattery` | `StatusBar/Battery_25x8.png` | 25×8 | ✓ available |
| `kIcCharging` | `StatusBar/Charging_lightning_9x10.png` | 9×10 | ✓ available |
| `kIcBleIdle` | `StatusBar/Bluetooth_Idle_5x8.png` | 5×8 | ✓ available |
| `kIcBleConnected` | `StatusBar/Bluetooth_Connected_16x8.png` | 16×8 | ✓ available |
| `kIcSdMounted` | `StatusBar/SDcardMounted_11x8.png` | 11×8 | ✓ available |
| `kIcSdFail` | `StatusBar/SDcardFail_11x8.png` | 11×8 | ✓ available |
| `kIcLock` | `StatusBar/Hidden_window_9x8.png` | 9×8 | ✓ available |
| `kIcWifi0` | — | ~10×8 | ⚠ **missing — needs custom artwork** |
| `kIcWifi1` | — | ~10×8 | ⚠ **missing — needs custom artwork** |
| `kIcWifi2` | — | ~10×8 | ⚠ **missing — needs custom artwork** |
| `kIcWifi3` | — | ~10×8 | ⚠ **missing — needs custom artwork** |

Flipper Zero has no WiFi hardware — no wifi icons in refs. WiFi icons must be
drawn by hand as pixel art or commissioned. Placeholder: use `kIcBleIdle` size
as template (10×8 px, 3 bars style).

### T1 UI Chrome

| Symbol | Source file | Size | Status |
|---|---|---|---|
| `kIcArrowUp` | `Interface/SmallArrowUp_3x5.png` | 3×5 | ✓ available |
| `kIcArrowDown` | `Interface/SmallArrowDown_3x5.png` | 3×5 | ✓ available |
| `kIcArrowLeft` | `Common/ButtonLeftSmall_3x5.png` | 3×5 | ✓ available |
| `kIcArrowRight` | `Common/ButtonRightSmall_3x5.png` | 3×5 | ✓ available |
| `kIcCheck` | `NFC/check_big_20x17.png` | 20×17 | ✓ available |

### T2 App Icon Animations (14×14, launcher use)

| Symbol | Source dir | Frames | Status |
|---|---|---|---|
| `kAnimIconSettings` | `MainMenu/Settings_14/` | 10 | ✓ available |
| `kAnimIconBadUsb` | `MainMenu/BadUsb_14/` | 11 | ✓ available |
| `kAnimIconNfc` | `MainMenu/NFC_14/` | 4 | ✓ available |
| `kAnimIconInfrared` | `MainMenu/Infrared_14/` | 6 | ✓ available |
| `kAnimIconApps` | `MainMenu/Plugins_14/` | 9 | ✓ available |
| `kAnimIconFiles` | `MainMenu/125khz_14/` | 4 | ⚠ **repurposed — no Files icon in refs** |
| `kAnimIconLogs` | — | — | ⚠ **missing — needs custom artwork** |
| `kAnimIconBadge` | — | — | ⚠ **missing — needs custom artwork** |

### T2 Loading Animations

| Symbol | Source dir | Size | Frames | Status |
|---|---|---|---|---|
| `kAnimSpinner` | `Common/Round_loader_8x8/` | 8×8 | 5 | ✓ available (replaces current hand-drawn spinner) |
| `kAnimLoading` | `Common/Loading_24/` | 24×24 | 7 | ✓ available |

### T3 Desktop Animations (move from C array → `.panim`)

| File | Source dir | Frames | Status |
|---|---|---|---|
| `/anims/doom.panim` | `dolphin/external/L1_Doom_128x64/` | 39 | ✓ available |
| `/anims/boxing.panim` | `dolphin/external/L1_Boxing_128x64/` | 16 | ✓ available |
| `/anims/sleep.panim` | `dolphin/external/L1_Sleep_128x64/` | 6 | ✓ available |
| `/anims/tv.panim` | `dolphin/external/L1_Tv_128x47/` | 8 | ✓ available |
| *(remaining 6)* | `dolphin/external/…` | varies | ✓ available |

---

## `tools/asset_gen/` — Bun/TypeScript

```
tools/asset_gen/
  package.json          (bun workspaces, deps: sharp)
  src/
    png2c.ts            PNG → C array header (T1/T2)
    seq2panim.ts        PNG sequence + meta.txt → .panim binary (T3)
    index.ts            CLI entry point
```

### Commands

```bash
# Convert single PNG to C array (T1 icon)
bun run asset_gen png2c StatusBar/Battery_25x8.png --out src/assets/ic_battery.h --sym kIcBattery

# Convert animation sequence to .panim (T3)
bun run asset_gen seq2panim dolphin/external/L1_Boxing_128x64/ --out data/anims/boxing.panim

# Batch convert all T1 icons (run at dev time, output committed)
bun run asset_gen batch --config asset_gen.config.json
```

---

## BadUSB → System App

**Change:** `rt.apps().install(badUsbApp)` in `main.cpp` targets → supply explicit
manifest with `category = "System"`.

```cpp
// main.cpp (all targets that include BadUSB)
static nema::BadUsbApp badUsbApp;
nema::AppManifest badUsbManifest{};
badUsbManifest.id       = badUsbApp.id();
badUsbManifest.name     = badUsbApp.name();
badUsbManifest.version  = "1.0.0";
badUsbManifest.kind     = nema::AppKind::BuiltIn;
badUsbManifest.type     = nema::AppType::App;
badUsbManifest.category = "System";          // ← key change
rt.apps().installCustom(badUsbApp, badUsbManifest);
```

Launcher (HomeScreen / DesktopLauncher) renders two sections:
- **System** — `category == "System"` entries (Files, Logs, Settings, BadUSB)
- **Apps** — everything else

---

## Task Checklist

### Phase 1 — Foundation

- [ ] Extend `Animation` struct with `framesOrder`, `passiveCount`, `activeCount`
- [ ] Update `AnimationPlayer` to use `playhead_` + `resolveFrame()`
- [ ] Add `triggerActive()` to `AnimationPlayer`
- [ ] Create `firmware/core/include/nema/assets/` directory structure
- [ ] Create `IAssetLoader` interface + `AssetStore` skeleton

### Phase 2 — Toolchain

- [ ] Create `tools/asset_gen/` with `package.json` (Bun)
- [ ] Implement `png2c.ts` (PNG → C array header, 1-bit output)
- [ ] Implement `seq2panim.ts` (PNG sequence + meta.txt → `.panim`)
- [ ] Test with `L1_Boxing_128x64/` → `boxing.panim`

### Phase 3 — T1 System Icons

- [ ] Run `png2c` on all ✓ status bar icons → `firmware/core/include/nema/assets/icons/`
- [ ] Create `system_icons.h` aggregate header with `kIc*` externs
- [ ] Wire `kIcBattery`, `kIcBleIdle`/`kIcBleConnected`, `kIcSdMounted`/`kIcSdFail`,
      `kIcLock` into status bar renderer (`gui_service.cpp`)
- [ ] ⚠ WiFi icons: draw placeholder 10×8 pixel art (3 bars, 4 levels) by hand as
      C array — mark `TODO: replace with final artwork`

### Phase 4 — T2 App Icon Animations

- [ ] Run `png2c` batch on `MainMenu/*_14/` sequences → `system_anims.h`
- [ ] Wire icon anims into launcher entries (LauncherEntry.icon → AnimationPlayer)
- [ ] Replace existing hand-drawn spinner with `kAnimSpinner` from `Round_loader_8x8`

### Phase 5 — T3 AssetLoader + Migration

- [ ] Implement `Esp32AssetLoader` (LittleFS) + `SimAssetLoader` (fopen)
- [ ] Implement `AssetStore::loadAnim()` / `freeAnim()`
- [ ] Run `seq2panim` on all 10 dolphin animations → `.panim` files
- [ ] Provision LittleFS partition with `.panim` files (ESP32 targets)
- [ ] Migrate `DolphinDemoScreen` from C-array to `loadAnim()` FS path
- [ ] Remove `dolphin_showcase.cpp` and `dolphin_anim.cpp` from build (874 KB saved)

### Phase 6 — System Apps

- [ ] Add `installBuiltin(IApp&, AppManifest)` overload to `AppRegistry`
      (or reuse `installCustom` — same signature, just clarify naming)
- [ ] Update BadUSB registration in all targets → `category = "System"`
- [ ] Update HomeScreen / launcher to render "System" section separately from "Apps"
- [ ] Verify Files, Logs, Settings already use `category = "System"` (audit + fix if not)

### Phase 7 — Documentation

- [ ] Write `docs/feats/assets.md` — full catalog, format spec, how-to guides
- [ ] Update `docs/STATE.md` — asset system row
- [ ] Tick plan checklist items above

---

## Missing Assets Summary (⚠)

| Asset | Reason missing | Resolution |
|---|---|---|
| `kIcWifi0`…`kIcWifi3` | Flipper has no WiFi hardware | Draw 10×8 pixel art (4 levels), TODO marker |
| `kAnimIconFiles` | No file-browser icon in refs | Repurpose `125khz_14` as placeholder |
| `kAnimIconLogs` | Not in refs | Draw simple 14×14 pixel art (scrolling lines) |
| `kAnimIconBadge` | Not in refs | Draw 14×14 pixel art (ID card / badge shape) |
