#pragma once
// Plan 71 — Flipper-Compatible Asset Loader.
// Hybrid approach: runtime .bm loader for bitmaps/animations + build-time font pipeline.
// Prioritas: performa > kemudahan developer > Flipper compat.
#include "nema/hal/filesystem.h"
#include "nema/ui/animation.h"
#include "nema/ui/canvas.h"
#include "nema/ui/icon_pack.h"
#include "nema/ui/font_registry.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace nema::asset {

// ── AssetArena — bump allocator ──────────────────────────────────────────────
// Plan 71: Fixed block allocator in PSRAM (ESP32) or heap (host). Zero
// fragmentation, O(1) reset. Screen transition resets the entire arena.
class AssetArena {
public:
    static AssetArena& instance();

    bool init(size_t sizeBytes = 256 * 1024);
    void shutdown();

    uint8_t* alloc(size_t size);
    void     reset();

    size_t used()     const { return offset_; }
    size_t capacity() const { return size_; }

private:
    AssetArena() = default;
    uint8_t* block_  = nullptr;
    size_t   size_   = 0;
    size_t   offset_ = 0;
};

// ── BitmapAsset — single .bm loader ──────────────────────────────────────────
// Flipper .bm = raw 1-bit pixel data, no header. Dimensions from filename
// convention ("name_WxH.bm") or explicit params. Data allocated from AssetArena.
struct BitmapAsset {
    std::vector<uint8_t> data;
    uint16_t width  = 0;
    uint16_t height = 0;

    bool load(IFileSystem& fs, const char* path);
    bool load(IFileSystem& fs, const char* path, uint16_t w, uint16_t h);

    const uint8_t* bits() const { return data.data(); }
    bool valid() const { return !data.empty() && width > 0 && height > 0; }
    void release();

    // Parse "name_WxH.bm" → w,h. Returns false if pattern not matched.
    static bool parseDimFromName(const char* path, uint16_t& w, uint16_t& h);
};

// Direct-from-VFS draw — reads file each call. Uses internal stack buffer.
// For infrequent icons; not suitable for per-frame animation reads.
bool drawFile(Canvas& c, IFileSystem& fs, const char* path,
              uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// ── AnimMeta — Flipper meta.txt parser ───────────────────────────────────────
struct AnimMeta {
    uint16_t width      = 0;
    uint16_t height     = 0;
    uint8_t  frameCount = 0;
    uint8_t  frameRate  = 0;   // fps
    bool     loop       = true;
};

AnimMeta parseAnimMeta(const char* txt);

// ── AnimAsset — multi-frame .bm directory loader ─────────────────────────────
struct AnimAsset {
    anim::Animation                 def;
    std::vector<anim::AnimationFrame> frames;
    std::vector<std::vector<uint8_t>> buffers;

    bool load(IFileSystem& fs, const char* dirPath);

    const anim::Animation& animation() const { return def; }
    bool valid() const { return def.frameCount > 0 && def.frames != nullptr; }
    void release();
};

// ── AssetPackLoader — Flipper pack directory structure ───────────────────────
class AssetPackLoader {
public:
    AssetPackLoader(IFileSystem& fs, const char* packPath);

    BitmapAsset loadIcon(const char* name);
    AnimAsset   loadAnimation(const char* name);

    std::vector<std::string> listIcons();
    std::vector<std::string> listAnimations();

    bool isValid() const;

private:
    IFileSystem& fs_;
    std::string  basePath_;
};

// ── AssetPackRegistry — handle-to-loaded-asset mapping ───────────────────────
// Maps icon handles to BitmapAsset data. Also provides IconDef lookup for
// integration with the existing icon system (findIcon() fallback).
class AssetPackRegistry {
public:
    static AssetPackRegistry& instance();

    void              registerIcon(const char* handle, const BitmapAsset& asset);
    const BitmapAsset* find(const char* handle) const;
    // Plan 71: IconDef bridge — returns an IconDef wrapping a registered
    // BitmapAsset. The returned pointer is valid as long as the BitmapAsset
    // is alive and registered.
    const aether::ui::IconDef* findIconDef(const char* handle) const;
    void              clear();

private:
    AssetPackRegistry() = default;

    struct Entry {
        std::string          handle;
        const BitmapAsset*   asset;
        mutable aether::ui::IconDef  def;   // IconDef wrapper rebuilt on lookup
    };
    std::vector<Entry> entries_;
};

} // namespace nema::asset

// ── Convenience: seed a demo asset pack into a filesystem ──────────────────────
namespace nema::asset {
void seedDemoAssets(IFileSystem& fs);
} // namespace nema::asset

