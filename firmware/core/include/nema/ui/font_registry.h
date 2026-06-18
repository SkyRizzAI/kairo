#pragma once
#include <cstdint>

namespace nema {

struct BitmapFont;   // forward — defined in canvas.h

namespace ui {

// Plan 70: Font handle — logical font name, resolved by FontRegistry.
// Apps and screen code reference fonts by handle, not raw pointer, so custom
// fonts from asset packs or per-app registrations are transparent.
using FontHandle = uint8_t;

// Named handles for built-in fonts. Custom fonts are registered at runtime
// and receive handles >= CUSTOM_BASE.
namespace Fonts {
    constexpr FontHandle Primary   = 0;
    constexpr FontHandle Secondary = 1;
    constexpr FontHandle Mono      = 2;
    constexpr FontHandle Tiny      = 3;
    constexpr FontHandle BigNum    = 4;
    constexpr FontHandle COUNT     = 5;
    constexpr FontHandle CUSTOM_BASE = 8;   // first runtime-registered slot
    constexpr FontHandle MAX         = 12;  // max total fonts
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
} // namespace nema
