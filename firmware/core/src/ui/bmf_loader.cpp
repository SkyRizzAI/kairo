#include "nema/ui/bmf_loader.h"
#include "nema/hal/filesystem.h"
#include <vector>
#include <cstring>

namespace nema::display {

LoadedFont::~LoadedFont() {
    delete[] data;
    delete[] widths;
    delete[] offsets;
}

LoadedFont* loadBmf(IFileSystem* fs, const char* path) {
    if (!fs) return nullptr;
    std::vector<uint8_t> raw;
    if (!fs->read(path, raw)) return nullptr;
    if (raw.size() < 12) return nullptr;
    const uint8_t* p = raw.data();
    if (p[0] != 0xBF || p[1] != 1) return nullptr;  // magic + version

    uint8_t  charW      = p[2];
    uint8_t  charH      = p[3];
    uint8_t  firstChar  = p[4];
    uint8_t  numChars   = p[5];
    uint8_t  spacing    = p[6];
    uint8_t  bpc        = p[7];
    bool     hasWidths  = p[8] != 0;
    bool     hasOffsets = p[9] != 0;
    uint16_t dataSize   = (uint16_t)p[10] | ((uint16_t)p[11] << 8);

    size_t expectedSize = 12u
        + (hasWidths  ? numChars       : 0u)
        + (hasOffsets ? numChars * 2u  : 0u)
        + dataSize;
    if (raw.size() < expectedSize) return nullptr;

    const uint8_t* cursor = p + 12;

    auto* lf    = new LoadedFont();
    lf->widths  = nullptr;
    lf->offsets = nullptr;
    lf->data    = nullptr;

    if (hasWidths) {
        lf->widths = new uint8_t[numChars];
        std::memcpy(lf->widths, cursor, numChars);
        cursor += numChars;
    }
    if (hasOffsets) {
        lf->offsets = new uint16_t[numChars];
        std::memcpy(lf->offsets, cursor, numChars * 2u);
        cursor += numChars * 2u;
    }
    lf->data = new uint8_t[dataSize];
    std::memcpy(lf->data, cursor, dataSize);

    lf->font = BitmapFont{
        lf->data, charW, charH, firstChar, numChars, spacing,
        lf->widths,
        lf->offsets,
        bpc
    };
    return lf;
}

} // namespace nema::display
