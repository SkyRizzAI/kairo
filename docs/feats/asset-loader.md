# Asset Loader & Animation Pipeline

> Plan 71 — Hybrid approach: runtime `.bm` loader + build-time font pipeline.

## Architecture

```
┌─ Asset Sources ──────────────────────────────────────┐
│  Compile-time (flash)        Runtime (VFS/SD)        │
│  ┌──────────┐               ┌──────────────────┐    │
│  │ icon_pack│               │ AssetPackLoader   │    │
│  │ anims    │               │  ├─ BitmapAsset   │    │
│  │ fonts    │               │  ├─ AnimAsset     │    │
│  └──────────┘               │  └─ AssetArena    │    │
│       │                     └────────┬─────────┘    │
│       └──────────┬───────────────────┘              │
│                  ▼                                  │
│     ┌────────────────────────┐                      │
│     │  Canvas / Renderer     │                      │
│     │  drawBitmap(ptr,w,h)   │  ← pointer-agnostic  │
│     └────────────────────────┘                      │
└─────────────────────────────────────────────────────┘
```

## Core Components

### BitmapAsset — Single `.bm` Loader

Flipper `.bm` = raw 1-bit pixel data, no header. Dimensions from filename convention (`icon_8x8.bm`) or explicit params.

```cpp
#include "nema/ui/asset_loader.h"

nema::asset::BitmapAsset icon;
if (icon.load(fs, "/packs/default/icons/wifi_8x8.bm"))
    c.drawBitmap(x, y, icon.width, icon.height, icon.bits());
```

### AnimAsset — Multi-Frame Loader

Loads Flipper-format animation directories:

```
/packs/default/animations/dolphin/
  ├─ meta.txt          ← Width, Height, Passive frames, Frame rate
  ├─ frame_0.bm
  ├─ frame_1.bm
  └─ ...
```

```cpp
nema::asset::AnimAsset anim;
if (anim.load(fs, "/packs/default/animations/dolphin")) {
    nema::anim::AnimationPlayer player(anim.animation());
    player.start();
    // ... tick & render
}
```

### AssetArena — Zero-Fragmentation Allocator

256KB bump allocator in PSRAM. O(1) reset per screen transition. Prevents heap fragmentation from repeated asset load/unload cycles.

```cpp
nema::asset::AssetArena::instance().init();  // boot
// ... use arena-backed allocations ...
nema::asset::AssetArena::instance().reset(); // screen transition
```

### AssetPackLoader — Flipper Pack Structure

Reads Flipper Momentum asset pack directory layout:

```
/packs/MyPack/
  ├─ icons/*.bm
  ├─ animations/<name>/meta.txt + frame_*.bm
  └─ fonts/*.u8f
```

```cpp
nema::asset::AssetPackLoader pack(fs, "/packs/MyPack");
auto icon = pack.loadIcon("icons/wifi_8x8.bm");
auto anim = pack.loadAnimation("animations/dolphin_idle");
```

### AssetPackRegistry — Icon Handle → VFS Bridge

Maps icon handles to VFS-loaded bitmaps. Falls back after built-in `findIcon()`:

```cpp
nema::asset::AssetPackRegistry::instance().registerIcon("custom.wifi", loadedIcon);
const IconDef* def = nema::ui::findIcon("custom.wifi");  // VFS fallback works
```

## Animation Lifecycle Rules

**CRITICAL**: Every `AnimationManager::registerPlayer()` MUST have a matching `unregisterPlayer()` before the player is destroyed. Failing to do so = dangling pointer = GUI thread crash.

```cpp
void onResume() override {
    player_ = std::make_unique<AnimationPlayer>(anim);
    player_->start();
    AnimationManager::instance().registerPlayer(*player_);
}

void onPause() override {
    if (player_) {
        AnimationManager::instance().unregisterPlayer(*player_);
        player_->stop();
    }
}
```

For custom apps (separate thread), do NOT use AnimationManager. Tick the player manually:

```cpp
bool onTick(AppContext& ctx) override {
    uint32_t now = ctx.runtime().clock().millis();
    if (player_->tick(now)) { dirty_ = true; return true; }
    return false;
}
```

## File Format Reference

### `.bm` — Flipper Bitmap

Raw 1-bit pixel data, row-major, MSB = leftmost pixel. `width * height / 8` bytes. No header, no magic bytes. Dimensions from filename convention (`name_WxH.bm`) or sidecar metadata.

### `meta.txt` — Flipper Animation Metadata

```
Width: 128
Height: 47
Passive frames: 6
Frame rate: 2
```

Fields used: `Width`, `Height`, `Passive frames`, `Frame rate`.

## Lessons Learned

1. **C++ `const` at namespace scope = internal linkage**. Use `extern const` for symbols that must survive WASM linker dead-code elimination.
2. **WASM linker (`wasm-ld`) aggressively strips `.a` archives**. Anchor symbols in always-linked TUs (`AnimationManager::instance()`) to keep them alive.
3. **AnimationManager is NOT thread-safe**. System screens use it from GUI thread only. Custom apps use manual ticking.
4. **AnimAsset must outlive AnimationPlayer**. The player stores a reference to `Animation`. Create player AFTER AnimAsset is in its final memory location.
5. **Don't touch the GUI loop or ViewDispatcher for asset loading**. Keep asset seeding at app/screen level, not in boot critical path.

## Files

| File | Purpose |
|------|---------|
| `firmware/core/include/nema/ui/asset_loader.h` | BitmapAsset, AnimAsset, AnimMeta, AssetPackLoader, AssetPackRegistry, AssetArena |
| `firmware/core/src/ui/asset_loader.cpp` | All loader implementations |
| `firmware/core/src/ui/asset_arena.cpp` | AssetArena singleton |
| `firmware/core/include/nema/ui/dolphin_anim.h` | DOLPHIN_TV Animation (system screen) + DOLPHIN_SHOWCASE (compiled-in blob) |
| `firmware/core/src/ui/dolphin_showcase.cpp` | Compiled-in dolphin frame data (10 animations) |
| `tools/fonts/encode.py` | BDF → BitmapFont converter script |
| `tools/fonts/sources/*.bdf` | u8g2 BDF font sources |
