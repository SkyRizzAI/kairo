#pragma once
#include "nema/hal/driver.h"
#include <string>
#include <vector>
#include <cstdint>

namespace nema {

// Volume capacity report returned by IFileSystem::statvfs().
// Backends that do not support capacity reporting return a zero-filled struct.
struct StatVfs {
    uint64_t totalBytes = 0;
    uint64_t freeBytes  = 0;
};

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
    virtual bool rename(const std::string& src, const std::string& dst) = 0;

    // Capacity report. Backends that don't support this return a zero StatVfs.
    virtual StatVfs statvfs() const { return {}; }

    // Streaming write (Plan 88 R5): write a large file chunk-by-chunk straight to
    // storage, so the device never buffers the whole file in RAM (which fragmented
    // the heap and wedged the device on big transfers). One stream at a time. Backends
    // that don't support it leave the defaults (false) and the caller falls back to a
    // single buffered write(). `offset` lets a retried chunk overwrite in place.
    virtual bool writeStreamBegin(const std::string& /*path*/) { return false; }
    virtual bool writeStreamChunk(uint32_t /*offset*/, const uint8_t* /*data*/, size_t /*len*/) { return false; }
    virtual bool writeStreamEnd() { return false; }
    virtual void writeStreamAbort() {}   // release a half-open stream (disconnect)

    // Recursively removes a path (file or non-empty directory).
    // Default implementation walks with list()+remove(); backends may override
    // if the underlying FS provides a native recursive-remove operation.
    virtual bool removeAll(const std::string& path) {
        std::vector<FsEntry> children;
        if (list(path, children)) {
            const bool slash = (path == "/");
            for (const auto& c : children) {
                std::string child = slash ? ("/" + c.name) : (path + "/" + c.name);
                if (c.isDir) removeAll(child);
                else         remove(child);
            }
        }
        return remove(path);
    }
};

} // namespace nema
