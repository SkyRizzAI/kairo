#pragma once
#include <cstdint>
#include <cstddef>

// Plan 59 — PAPP1: Palanu App Package v1.
//
// Two wire formats, one magic prefix ("PAPP1\n"):
//
//   Single-file (no assets):
//     "PAPP1\n"
//     <manifest-json-line>\n
//     <code-bundle (js text | wasm bytes)>
//
//   Bundle (TOC-concatenated):
//     "PAPP1\n"
//     u16le  entryCount
//     TOC[entryCount]:
//       u8     nameLen
//       u8[nameLen]  name  ("manifest.json" | "app.js" | "icon.xbm" | ...)
//       u8     flags  (bit0 = RLE-compressed, Plan 35 codec)
//       u32le  length   (decompressed / original size)
//       u32le  stored   (bytes in file; == length if uncompressed)
//     blob[0] blob[1] ... (concatenated, one per TOC entry in order)
//
// manifest.json MUST be TOC entry 0 in a bundle so the launcher can read
// metadata without loading the code/assets.
//
// Detection: after the 6-byte magic, '{' → single-file (JSON manifest line);
// any other byte → bundle (low byte of u16le entryCount).

namespace nema {

static constexpr const char kPapp1Magic[] = "PAPP1\n";
static constexpr size_t     kPapp1MagicLen = 6;

// Flags for a TOC entry.
static constexpr uint8_t kPapp1FlagRle = 0x01;

// One entry in a parsed PAPP1 TOC.
struct PappEntry {
    const char*    name;     // NOT null-terminated; use nameLen for length
    uint8_t        nameLen;  // byte count for `name`
    const uint8_t* data;     // pointer into the original buffer (stored bytes)
    uint32_t       length;   // original (decompressed) size in bytes
    uint32_t       stored;   // bytes present in `data`
    uint8_t        flags;    // kPapp1FlagRle etc.
};

// Result of parsePapp(). At most kPappMaxEntries entries.
static constexpr size_t kPappMaxEntries = 16;

struct PappPackage {
    bool       valid       = false;
    bool       singleFile  = false;  // true = single-file (no TOC)

    // Single-file fields (valid when singleFile == true):
    const char*    manifestJson = nullptr;  // null-terminated JSON string
    const uint8_t* bundle       = nullptr;  // code bytes (js text or wasm)
    size_t         bundleLen    = 0;

    // TOC fields (valid when singleFile == false):
    PappEntry  entries[kPappMaxEntries];
    size_t     entryCount = 0;

    // Convenience: find an entry by name. Returns nullptr if not found.
    const PappEntry* find(const char* name) const;
};

// Parse `data[0..len)` in-place (no copies). Returned PappPackage borrows
// pointers into `data`, which must outlive the result. Returns .valid=false
// on any format error (bad magic, truncated TOC, overflow).
PappPackage parsePapp(const uint8_t* data, size_t len);

} // namespace nema
