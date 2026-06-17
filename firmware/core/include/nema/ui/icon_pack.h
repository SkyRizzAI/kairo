#pragma once
// Plan 53 — Built-in icon handle system.
// Provides a fixed set of 8x8 1-bit XBM bitmaps for status/feature/file/action
// icons. Reference icons by handle string ("status.wifi", "feature.apps", etc.)
// and draw with aether::ui::draw::icon().
#include <cstdint>

namespace nema::ui {

struct IconDef {
    const char*    handle;   // e.g. "status.wifi"
    const uint8_t* bitmap;   // 1-bit packed, row-major, MSB first
    uint8_t        w;        // pixels wide
    uint8_t        h;        // pixels tall
};

// Lookup by handle. Returns nullptr if not found.
const IconDef* findIcon(const char* handle);

// Null-terminated list of all built-in icons (for enumeration).
const IconDef* const* allIcons();

} // namespace nema::ui
