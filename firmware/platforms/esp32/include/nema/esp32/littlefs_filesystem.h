#pragma once
#include "nema/hal/filesystem.h"
#include <string>

namespace nema {

// LittleFsFileSystem — persistent IFileSystem backend on the ESP32 internal flash
// (Plan 38). Mounts the "spiffs" data partition as LittleFS (power-fail safe +
// wear-leveling) via esp_vfs_littlefs, then maps the IFileSystem calls onto POSIX
// (opendir/stat/fopen/mkdir/unlink) under a base path. Files written here survive
// reboots. ESP32-only (POSIX + esp_littlefs), so it lives in the platform, not
// core — the VFS mounts it at "/" exactly like the volatile MemFileSystem.
class LittleFsFileSystem : public IFileSystem {
public:
    // Mount (and format on first boot). Returns false if the partition is missing
    // or mount+format both fail. `partitionLabel` matches partitions.csv.
    bool begin(const char* partitionLabel = "spiffs", const char* basePath = "/lfs");
    bool mounted() const { return mounted_; }

    const char* name() const override { return "LittleFS"; }

    bool list  (const std::string& path, std::vector<FsEntry>& out) override;
    bool read  (const std::string& path, std::vector<uint8_t>& out) override;
    bool write (const std::string& path, const uint8_t* data, size_t len) override;
    bool mkdir (const std::string& path) override;
    bool remove(const std::string& path) override;

private:
    std::string real(const std::string& vpath) const;  // VFS path → POSIX path
    void mkdirsFor(const std::string& realPath);        // mkdir -p of ancestors

    std::string base_;
    bool        mounted_ = false;
};

} // namespace nema
