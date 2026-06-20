# Dolphin Animation — Custom App

> Plan 71 — Dynamic VFS-loaded dolphin animation showcase (custom/installable app).

## Overview

`DolphinApp` is a **custom installable app** (`com.palanu.dolphin`) that demonstrates dynamic asset loading. It seeds the VFS with `.bm` frame files at launch, then loads all animations via `AnimAsset::load()` from the filesystem.

This proves the asset loader pipeline works for user-installed apps — not just compiled-in system screens.

## Key Difference from System Screen

| Aspect | System Screen | Custom App |
|--------|--------------|-----------|
| Access | HomeScreen carousel | AppList → launch |
| Data source | Compiled-in `Animation` structs | VFS `.bm` files via `AnimAsset::load()` |
| Timing | `AnimationManager` (GUI thread) | Manual `player->tick()` in `onTick()` (app thread) |
| Thread | GUI thread | Own thread via `AppHost` |
| Lifecycle | `onResume/onPause` | `onStart/key/onTick/drawRaw` |
| Install | Hardcoded in carousel | `rt.apps().install(dolphinApp)` |
| Reload | Need recompile | Drop new `.bm` on SD card |

## File Reference

| File | Purpose |
|------|---------|
| `firmware/core/include/nema/apps/dolphin_app.h` | App class declaration |
| `firmware/core/src/apps/dolphin_app.cpp` | App implementation (VFS seeding, load, tick, render) |

## Pipeline

```
DolphinApp::onStart()
  │
  ├─ seedDolphinAssets(ctx)
  │   └─ Write compiled-in frame data to VFS:
  │       /packs/dolphin/<name>/
  │         ├─ frame_0.bm ... frame_N.bm
  │         └─ meta.txt
  │
  ├─ For each animation:
  │   ├─ AnimAsset::load(fs, "/packs/dolphin/<name>")
  │   │   ├─ Parse meta.txt → dimensions, frame count, FPS
  │   │   ├─ Read frame_N.bm files → pixel buffers
  │   │   └─ Build AnimationFrame[] + Animation struct
  │   │
  │   └─ AnimationPlayer(anim.animation())
  │       └─ start() — begins playback
  │
DolphinApp::onTick()
  └─ player->tick(ctx.runtime().clock().millis())
      └─ Advances frame based on elapsed time and FPS

DolphinApp::drawRaw()
  └─ Render current frame scaled to fill screen
```

## Thread Safety

The app runs in its own thread via `AppHost`. `AnimationManager` is NOT used — players are ticked manually from `onTick()` to avoid cross-thread access to `AnimationManager::players_` vector.

```cpp
// App thread — safe, no cross-thread access
bool onTick(AppContext& ctx) override {
    uint32_t now = ctx.runtime().clock().millis();
    if (player_->tick(now)) {
        dirty_ = true;
        return true;
    }
    return false;
}
```

## Controls

| Input | Action |
|-------|--------|
| Left / Up | Previous animation |
| Right / Down | Next animation |
| Select (OK) | Pause / Resume |
| Cancel (Back) | Exit app |

## Registration

In target `main.cpp`:

```cpp
#include "nema/apps/dolphin_app.h"

static nema::DolphinApp dolphinApp;
rt.apps().install(dolphinApp);
```

App appears in AppList as **"Dolphin Showcase"**.

## Memory

- Frame data seeded to VFS from compiled-in blob (894KB source, 250KB raw pixel data)
- Each `AnimAsset` owns its frame buffers (heap-allocated `vector<uint8_t>`)
- `AnimationPlayer` stores only state (~40 bytes) + reference to `Animation` struct
- No `AssetArena` dependency — app uses heap/VFS directly

## Extending

To make the app load from SD card instead of compiled-in seed data:

1. Place `.bm` files + `meta.txt` on SD card at `/packs/dolphin/<name>/`
2. Remove the `seedDolphinAssets()` call
3. The existing `AnimAsset::load()` calls will pick up files from SD card VFS

This is the intended production path — compiled-in seed is for prototyping only.
