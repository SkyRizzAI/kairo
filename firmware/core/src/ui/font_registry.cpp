#include "nema/ui/font_registry.h"
#include "nema/ui/bmf_loader.h"
#include "nema/hal/filesystem.h"
#include <cstring>
#include <string>

namespace nema::display {

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
    if (handle >= Fonts::MAX) return;
    if (owned_[handle]) {
        delete owned_[handle];
        owned_[handle] = nullptr;
    }
    entries_[handle].font = nullptr;
    entries_[handle].name = nullptr;
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

bool FontRegistry::loadFontFile(FontHandle h, nema::IFileSystem* fs,
                                const char* path, const char* name) {
    if (h >= Fonts::MAX) return false;
    LoadedFont* lf = loadBmf(fs, path);
    if (!lf) return false;
    // Free any previously owned font at this slot
    if (owned_[h]) {
        delete owned_[h];
        owned_[h] = nullptr;
    }
    owned_[h] = lf;
    registerFont(h, &lf->font, name);
    return true;
}

bool FontRegistry::applyFontPack(nema::IFileSystem* fs, const char* dirPath) {
    if (!fs || !dirPath) return false;
    std::string base(dirPath);
    if (!base.empty() && base.back() != '/') base += '/';

    bool any = false;
    any |= loadFontFile(Fonts::Reg8,   fs, (base + "reg8.bmf").c_str(),   "reg8");
    any |= loadFontFile(Fonts::Bold8,  fs, (base + "bold8.bmf").c_str(),  "bold8");
    any |= loadFontFile(Fonts::Reg10,  fs, (base + "reg10.bmf").c_str(),  "reg10");
    any |= loadFontFile(Fonts::Bold10, fs, (base + "bold10.bmf").c_str(), "bold10");
    any |= loadFontFile(Fonts::Reg12,  fs, (base + "reg12.bmf").c_str(),  "reg12");
    any |= loadFontFile(Fonts::Bold12, fs, (base + "bold12.bmf").c_str(), "bold12");

    if (any) {
        // Re-map role handles to the newly loaded size/weight fonts.
        // Only update if the target slot actually has a font loaded.
        if (entries_[Fonts::Bold10].font)
            registerFont(Fonts::Primary,   entries_[Fonts::Bold10].font, "primary");
        if (entries_[Fonts::Reg8].font) {
            registerFont(Fonts::Secondary, entries_[Fonts::Reg8].font,   "secondary");
            registerFont(Fonts::Tiny,      entries_[Fonts::Reg8].font,   "tiny");
        }
        if (entries_[Fonts::Bold12].font)
            registerFont(Fonts::BigNum,    entries_[Fonts::Bold12].font, "bignum");
    }
    return any;
}

int FontRegistry::scanFontPacks(nema::IFileSystem* fs, const char* basePath,
                                char outNames[][48], char outPaths[][96], int maxPacks) {
    if (!fs || !basePath || !outNames || !outPaths || maxPacks <= 0) return 0;

    std::vector<nema::FsEntry> entries;
    if (!fs->list(basePath, entries)) return 0;

    std::string base(basePath);
    if (!base.empty() && base.back() != '/') base += '/';

    int count = 0;
    for (const auto& e : entries) {
        if (!e.isDir) continue;
        if (count >= maxPacks) break;
        // Copy name (truncate to 47 chars + NUL)
        std::strncpy(outNames[count], e.name.c_str(), 47);
        outNames[count][47] = '\0';
        // Build full path
        std::string full = base + e.name;
        std::strncpy(outPaths[count], full.c_str(), 95);
        outPaths[count][95] = '\0';
        count++;
    }
    return count;
}

} // namespace nema::display
