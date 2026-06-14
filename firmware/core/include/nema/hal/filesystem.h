#pragma once
#include "nema/hal/driver.h"
#include <string>
#include <vector>
#include <cstdint>

namespace nema {

// One directory entry.
struct FsEntry {
    std::string name;   // basename only
    bool        isDir = false;
    uint32_t    size  = 0;   // bytes (0 for directories)
};

// IFileSystem — storage HAL (Plan 38). One interface, many backends, chosen at
// runtime by capability ("storage.*"), never by board type:
//   - MemFileSystem  — in-RAM, volatile. Used by the WASM simulator and as a
//                      stopgap on hardware until the LittleFS backend lands.
//   - (future) LittleFS internal flash, FAT microSD.
//
// v1 is whole-file read/write — enough for the simulator file browser and small
// configs/apps. Chunked/offset I/O for large files is a planned extension (the
// PLP File channel reserves room for it); paths are absolute, '/'-separated.
struct IFileSystem : IDriver {
    DriverKind kind() const override { return DriverKind::Storage; }

    virtual bool list  (const std::string& path, std::vector<FsEntry>& out) = 0;
    virtual bool read  (const std::string& path, std::vector<uint8_t>& out) = 0;
    virtual bool write (const std::string& path, const uint8_t* data, size_t len) = 0;
    virtual bool mkdir (const std::string& path) = 0;
    virtual bool remove(const std::string& path) = 0;
};

} // namespace nema
