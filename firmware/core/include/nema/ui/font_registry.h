#pragma once
#include <cstdint>

namespace nema {
struct IFileSystem;
}

namespace nema::display { struct BitmapFont; }   // defined in canvas.h
namespace nema::display { struct LoadedFont; }   // defined in bmf_loader.h

namespace nema::display {

// Plan 70: Font handle — logical font name, resolved by FontRegistry.
// Apps and screen code reference fonts by handle, not raw pointer, so custom
// fonts from asset packs or per-app registrations are transparent.
using FontHandle = uint8_t;

// Named handles for built-in fonts. Custom fonts are registered at runtime
// and receive handles >= CUSTOM_BASE.
namespace Fonts {
    // Role handles — what TextRole maps to (see text_style.cpp). These point at
    // the proportional Helvetica family below.
    constexpr FontHandle Primary   = 0;   // bold (subheaders / titles)
    constexpr FontHandle Secondary = 1;   // regular (body / list items)
    constexpr FontHandle Mono      = 2;   // monospace (CLI / terminal)
    constexpr FontHandle Tiny      = 3;   // smallest regular
    constexpr FontHandle BigNum    = 4;   // large bold (clocks / numbers)
    // Explicit size/weight handles — for Layer-3 components that pick a precise
    // metric (e.g. ListView subheader = Bold10, item = Reg8).
    constexpr FontHandle Reg8      = 5;
    constexpr FontHandle Bold8     = 6;
    constexpr FontHandle Reg10     = 7;
    constexpr FontHandle Bold10    = 8;
    constexpr FontHandle Reg12     = 9;
    constexpr FontHandle Bold12    = 10;
    constexpr FontHandle COUNT     = 11;
    constexpr FontHandle CUSTOM_BASE = 16;  // first runtime-registered slot
    constexpr FontHandle MAX         = 20;  // max total fonts
}

// FontRegistry — singleton that owns all loaded font data.
// Register built-in fonts at boot; load custom fonts from asset packs later.
class FontRegistry {
public:
    static FontRegistry& instance();

    // Register a font under a handle. For built-ins, use Fonts::Primary etc.
    // For custom fonts, returns the assigned handle (>= CUSTOM_BASE).
    void registerFont(FontHandle handle, const BitmapFont* font, const char* name);
    FontHandle registerCustom(const BitmapFont* font, const char* name);

    void unregisterFont(FontHandle handle);

    // Lookup
    const BitmapFont* get(FontHandle handle) const;
    FontHandle findByName(const char* name) const;
    bool has(FontHandle handle) const;
    const char* nameOf(FontHandle handle) const;

    // Dynamic font loading — load a .bmf file from the filesystem and register
    // it at the given handle. FontRegistry takes ownership of the LoadedFont.
    bool loadFontFile(FontHandle h, nema::IFileSystem* fs, const char* path, const char* name);

    // Load a complete 6-file font pack (reg8/bold8/reg10/bold10/reg12/bold12)
    // from dirPath and re-register the role handles (Primary, Secondary, etc.).
    // Returns true if at least one font loaded successfully.
    bool applyFontPack(nema::IFileSystem* fs, const char* dirPath);

    // Enumerate available font packs under basePath (subdirectory names).
    // Returns the number of packs found (up to maxPacks).
    // outNames[][48] receives the pack name; outPaths[][96] receives full path.
    int scanFontPacks(nema::IFileSystem* fs, const char* basePath,
                      char outNames[][48], char outPaths[][96], int maxPacks);

private:
    FontRegistry() = default;

    struct Entry {
        const BitmapFont* font = nullptr;
        const char*       name = nullptr;
    };
    Entry      entries_[Fonts::MAX];
    LoadedFont* owned_[Fonts::MAX] = {};  // owns heap fonts; nullptr = not owned
    FontHandle nextCustom_ = Fonts::CUSTOM_BASE;
};

} // namespace ui

