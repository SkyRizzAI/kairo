#pragma once
#include "nema/ui/canvas.h"
#include <cstdint>

namespace nema {
struct IFileSystem;

namespace display {

// Owns heap-allocated font data loaded from a .bmf file.
struct LoadedFont {
    BitmapFont font;     // public BitmapFont (points into owned buffers below)
    uint8_t*   data;     // heap-allocated bitmap data
    uint8_t*   widths;   // heap-allocated widths[] or nullptr
    uint16_t*  offsets;  // heap-allocated offsets[] or nullptr

    // Non-copyable
    LoadedFont(const LoadedFont&)            = delete;
    LoadedFont& operator=(const LoadedFont&) = delete;
    LoadedFont()                             = default;
    ~LoadedFont();
};

// Load a .bmf file from the filesystem.
// Returns heap-allocated LoadedFont on success, nullptr on any error.
// Caller owns the returned pointer and must delete it when done.
LoadedFont* loadBmf(IFileSystem* fs, const char* path);

} // namespace display
} // namespace nema
