#pragma once
// Plan 71 — Flipper Zero dolphin animation showcase.
// Plan 82 Phase 5 — migrated from C array (dolphin_showcase.cpp) to .panim
// files loaded at runtime from the filesystem, removing ~900 KB from the binary.
#include <cstddef>
#include <cstdint>

namespace nema::anim {

struct DolphinEntry {
    const char* name;        // display name
    const char* path;        // path relative to VFS root (e.g. "anims/doom.panim")
    uint16_t    w, h;        // frame size (from meta, for display info)
    uint8_t     fps;
};

extern const DolphinEntry DOLPHIN_ENTRIES[];
extern const size_t       DOLPHIN_ENTRIES_COUNT;

} // namespace nema::anim
