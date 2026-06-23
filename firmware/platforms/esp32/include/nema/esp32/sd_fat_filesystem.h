#pragma once
#include "nema/hal/filesystem.h"
#include "nema/board.h"     // SdSpiConfig
#include "nema/log/logger.h"
#include <string>

namespace nema {

// SdFatFileSystem — IFileSystem backend for a microSD card over SPI (Plan 38).
// Mounts a FAT volume via esp_vfs_fat_sdspi_mount, then maps the IFileSystem
// calls onto POSIX (opendir/stat/fopen/mkdir/unlink) under a base path — exactly
// like LittleFsFileSystem, just a different mount. The VFS mounts it at "/sd".
// ESP32-only; the mount is non-fatal (no card → begin() returns false, boot
// continues and "/sd" simply doesn't appear).
class SdFatFileSystem : public IFileSystem {
public:
    bool begin(const SdSpiConfig& cfg, const char* basePath = "/sdcard");
    void attachLogger(Logger& log) { log_ = &log; }
    bool mounted()  const { return mounted_; }
    const char* basePath() const { return base_.c_str(); }

    const char* name() const override { return "SD (FAT)"; }

    bool list  (const std::string& path, std::vector<FsEntry>& out) override;
    bool read  (const std::string& path, std::vector<uint8_t>& out) override;
    bool write (const std::string& path, const uint8_t* data, size_t len) override;
    bool mkdir (const std::string& path) override;
    bool remove(const std::string& path) override;
    bool rename(const std::string& src, const std::string& dst) override;
    StatVfs statvfs() const override;

    bool writeStreamBegin(const std::string& path) override;
    bool writeStreamChunk(uint32_t offset, const uint8_t* data, size_t len) override;
    bool writeStreamEnd() override;
    void writeStreamAbort() override;

private:
    std::string real(const std::string& vpath) const;  // VFS path → POSIX path
    void mkdirsFor(const std::string& realPath);        // mkdir -p of ancestors

    Logger*     log_     = nullptr;
    std::string base_;
    bool        mounted_ = false;
    void*       card_    = nullptr;   // sdmmc_card_t* (opaque; IDF type stays in .cpp)
    void*       stream_  = nullptr;   // FILE* of the active streaming write (opaque)
};

} // namespace nema
