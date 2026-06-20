#include "nema/ui/font_registry.h"
#include <string>

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

FontRegistry& FontRegistry::instance() {
    static FontRegistry reg;
    return reg;
}

void FontRegistry::registerFont(FontHandle handle, const BitmapFont* font, const char* name) {
    if (handle < Fonts::MAX) {
        entries_[handle].font = font;
        entries_[handle].name = name;
    }
}

FontHandle FontRegistry::registerCustom(const BitmapFont* font, const char* name) {
    if (nextCustom_ < Fonts::MAX) {
        FontHandle h = nextCustom_++;
        entries_[h].font = font;
        entries_[h].name = name;
        return h;
    }
    return Fonts::Secondary;   // fallback: no slot left
}

void FontRegistry::unregisterFont(FontHandle handle) {
    if (handle >= Fonts::CUSTOM_BASE && handle < Fonts::MAX) {
        entries_[handle].font = nullptr;
        entries_[handle].name = nullptr;
    }
}

const BitmapFont* FontRegistry::get(FontHandle handle) const {
    if (handle < Fonts::MAX && entries_[handle].font)
        return entries_[handle].font;
    return entries_[Fonts::Secondary].font;   // safe fallback
}

FontHandle FontRegistry::findByName(const char* name) const {
    if (!name) return Fonts::Secondary;
    for (uint8_t i = 0; i < Fonts::MAX; i++)
        if (entries_[i].name && std::string(entries_[i].name) == name)
            return i;
    return Fonts::Secondary;
}

bool FontRegistry::has(FontHandle handle) const {
    return handle < Fonts::MAX && entries_[handle].font;
}

const char* FontRegistry::nameOf(FontHandle handle) const {
    if (handle < Fonts::MAX && entries_[handle].name)
        return entries_[handle].name;
    return "unknown";
}

} // namespace aether::ui
