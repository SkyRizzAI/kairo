// Plan 71 — AssetArena singleton. Bump allocator backed by PSRAM on ESP32,
// plain heap on host. Zero fragmentation, O(1) reset per screen transition.
#include "nema/ui/asset_loader.h"
#include <cstdlib>
#include <cstring>

#ifdef ESP_PLATFORM
  #include <esp_heap_caps.h>
#endif

namespace nema::asset {

// ── Alloc / free helpers ────────────────────────────────────────────────────

static uint8_t* arenaMalloc(size_t n) {
#ifdef ESP_PLATFORM
    if (auto* p = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM)) return p;
    return (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_8BIT);
#else
    return (uint8_t*)std::malloc(n);
#endif
}

static void arenaFree(uint8_t* p) {
#ifdef ESP_PLATFORM
    if (p) heap_caps_free(p);
#else
    std::free(p);
#endif
}

// ── AssetArena ──────────────────────────────────────────────────────────────

AssetArena& AssetArena::instance() {
    static AssetArena inst;
    return inst;
}

bool AssetArena::init(size_t sizeBytes) {
    if (block_) return true;   // already initialised
    if (sizeBytes == 0) return false;

    block_ = arenaMalloc(sizeBytes);
    if (!block_) return false;

    size_   = sizeBytes;
    offset_ = 0;
    return true;
}

void AssetArena::shutdown() {
    if (block_) {
        arenaFree(block_);
        block_  = nullptr;
        size_   = 0;
        offset_ = 0;
    }
}

uint8_t* AssetArena::alloc(size_t size) {
    if (!block_ || offset_ + size > size_) return nullptr;
    uint8_t* p = block_ + offset_;
    offset_ += size;
    return p;
}

void AssetArena::reset() {
    offset_ = 0;
    if (block_)
        std::memset(block_, 0, size_);
}

} // namespace nema::asset
