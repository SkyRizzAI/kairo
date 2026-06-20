# Dolphin Animation — System Screen

> Plan 71 — Fullscreen Flipper Zero dolphin animation showcase (system screen).

## Overview

`DolphinDemoScreen` renders Flipper Zero dolphin animations fullscreen from compiled-in data. Accessible via HomeScreen carousel. Uses `AnimationManager` for timing (GUI thread).

## File Reference

| File | Purpose |
|------|---------|
| `firmware/core/include/nema/screens/dolphin_demo.h` | Screen class declaration |
| `firmware/core/src/screens/dolphin_demo.cpp` | Screen implementation (draw, input, lifecycle) |
| `firmware/core/include/nema/ui/dolphin_anim.h` | Animation declarations (DOLPHIN_SHOWCASE, DOLPHIN_META) |
| `firmware/core/src/ui/dolphin_showcase.cpp` | Compiled-in frame data (894KB, 10 animations, 142 frames) |

## Usage

Access from HomeScreen carousel (item "Dolphin"). The screen loads all 10 Flipper Zero animations at boot:

| # | Animation | Frames | Size | Source |
|---|-----------|--------|------|--------|
| 1 | L1_Doom_128x64 | 15 | 128×64 | Flipper Zero (GPL-3.0) |
| 2 | L1_Kaiju_128x64 | 16 | 128×64 | Flipper Zero (GPL-3.0) |
| 3 | L1_My_dude_128x64 | 37 | 128×64 | Flipper Zero (GPL-3.0) |
| 4 | L1_Painting_128x64 | 9 | 128×64 | Flipper Zero (GPL-3.0) |
| 5 | L1_Read_books_128x64 | 13 | 128×64 | Flipper Zero (GPL-3.0) |
| 6 | L1_Senpai_128x64 | 16 | 128×64 | Flipper Zero (GPL-3.0) |
| 7 | L1_Showtime_128x64 | 26 | 128×64 | Flipper Zero (GPL-3.0) |
| 8 | L1_Sleep_128x64 | 2 | 128×64 | Flipper Zero (GPL-3.0) |
| 9 | L1_Tv_128x47 | 6 | 128×47 | Flipper Zero (GPL-3.0) |
| 10 | L1_Waves_128x50 | 2 | 128×50 | Flipper Zero (GPL-3.0) |

## Controls

| Input | Action |
|-------|--------|
| Left / Up | Previous animation |
| Right / Down | Next animation |
| Select (OK) | Pause / Resume |
| Cancel (Back) | Exit to HomeScreen |

## Display

Each animation is scaled up to fill the screen while maintaining aspect ratio (max 5×). Banner shows animation name. Bottom bar shows index, dimensions, FPS, frame count, and play/pause state.

## Lifecycle

```cpp
void onResume() override {
    loadCurrent();  // creates AnimationPlayer, registers with AnimationManager
}

void onPause() override {
    // Unregisters and stops player — CRITICAL for preventing GUI crash
    AnimationManager::instance().unregisterPlayer(*player_);
    player_->stop();
}
```

Uses `AnimationManager` on GUI thread for synchronized frame advancement. Proper `register/unregister` pairing prevents dangling pointer crashes.

## Architecture Notes

- Compiled-in frame data via `dolphin_showcase.cpp` (894KB, 10 animations)
- Each animation stored as `extern const Animation` for WASM linker compatibility
- `kDolphinBlob[]` — single concatenated binary blob of all frame pixel data
- `kDolphinFrames[]` — per-frame metadata (offset + dimensions)
- `DOLPHIN_SHOWCASE[]` — array of Animation pointers for iteration
- `DOLPHIN_META[]` — metadata per animation (name, dimensions, fps, frames)

## Adding New Animations

1. Download PNG frames from Flipper Zero repo and `meta.txt`
2. Convert to `.bm` via `tools/dolphin/convert_frames.py`
3. Regenerate `dolphin_showcase.cpp` via the generator script
4. Update `DOLPHIN_SHOWCASE_COUNT` in `dolphin_anim.h`
