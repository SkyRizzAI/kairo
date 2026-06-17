#include "nema/app/papp_package.h"
#include <cstring>

namespace nema {

const PappEntry* PappPackage::find(const char* name) const {
    if (!name) return nullptr;
    size_t nlen = std::strlen(name);
    for (size_t i = 0; i < entryCount; i++) {
        if (entries[i].name && entries[i].nameLen == (uint8_t)nlen &&
                std::memcmp(entries[i].name, name, nlen) == 0)
            return &entries[i];
    }
    return nullptr;
}

PappPackage parsePapp(const uint8_t* data, size_t len) {
    PappPackage pkg;

    // Magic check: "PAPP1\n"
    if (len < kPapp1MagicLen) return pkg;
    if (std::memcmp(data, kPapp1Magic, kPapp1MagicLen) != 0) return pkg;

    const uint8_t* p   = data + kPapp1MagicLen;
    const uint8_t* end = data + len;

    if (p >= end) return pkg;

    // Detection: '{' → single-file (JSON manifest follows); anything else → bundle TOC.
    if (*p == '{') {
        pkg.singleFile   = true;

        // Find the end of the manifest JSON line ('\n').
        const uint8_t* nl = static_cast<const uint8_t*>(std::memchr(p, '\n', (size_t)(end - p)));
        if (!nl) return pkg;  // no newline = truncated

        // Temporarily null-terminate in-place is not possible (const data).
        // Instead expose as a pointer + length; caller reads until '\n'.
        // For convenience, the manifest line must be null-terminated by the
        // bundle builder (which pads a '\0' after the '\n'). If not, caller
        // uses bundleLen to know where the manifest JSON ends.
        pkg.manifestJson = reinterpret_cast<const char*>(p);

        // Code bundle starts after the newline.
        const uint8_t* codeStart = nl + 1;
        if (codeStart > end) return pkg;
        pkg.bundle    = codeStart;
        pkg.bundleLen = (size_t)(end - codeStart);
        pkg.valid     = true;
        return pkg;
    }

    // Bundle format: u16le entryCount.
    if (p + 2 > end) return pkg;
    uint16_t entryCount = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
    p += 2;

    if (entryCount > kPappMaxEntries) return pkg;

    // Parse TOC entries.
    for (uint16_t i = 0; i < entryCount; i++) {
        if (p >= end) return pkg;

        uint8_t nameLen = *p++;
        if (p + nameLen > end) return pkg;

        pkg.entries[i].name    = reinterpret_cast<const char*>(p);
        pkg.entries[i].nameLen = nameLen;
        p += nameLen;

        if (p + 1 + 4 + 4 > end) return pkg;
        pkg.entries[i].flags  = *p++;
        uint32_t length = (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
        p += 4;
        uint32_t stored = (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
        p += 4;
        pkg.entries[i].length = length;
        pkg.entries[i].stored = stored;
        // data pointer filled in the second pass below.
    }

    pkg.entryCount = entryCount;

    // Second pass: assign blob pointers (they follow the TOC sequentially).
    for (size_t i = 0; i < entryCount; i++) {
        if (p + pkg.entries[i].stored > end) return pkg;
        pkg.entries[i].data = p;
        p += pkg.entries[i].stored;
    }

    pkg.singleFile = false;
    pkg.valid      = true;
    return pkg;
}

} // namespace nema
