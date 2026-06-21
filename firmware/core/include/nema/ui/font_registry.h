#pragma once
#include <cstdint>

namespace nema::display { struct BitmapFont; }   // defined in canvas.h

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

private:
    FontRegistry() = default;

    struct Entry {
        const BitmapFont* font = nullptr;
        const char*       name = nullptr;
    };
    Entry entries_[Fonts::MAX];
    FontHandle nextCustom_ = Fonts::CUSTOM_BASE;
};

} // namespace ui

