// Plan 83 — AppStorage implementation.
#include "nema/fs/app_storage.h"
#include <cstdint>
#include <cstring>

namespace nema {

// djb2 hash → 8-char hex — same function as nema_host_impl.cpp.
// Kept local to avoid coupling; this is the only cross-file contract.
static std::string storNsKey(const std::string& bundleId) {
    uint32_t h = 5381;
    for (char c : bundleId) h = ((h << 5) + h) + (uint8_t)c;
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", (unsigned int)h);
    return buf;
}

AppStorage::AppStorage(std::string bundleId, IFileSystem* vfs,
                       IConfigStore& cfg, bool critical, bool forceExternal)
    : bundleId_(std::move(bundleId)), vfs_(vfs), cfg_(cfg), critical_(critical) {
    // Base path resolved here (not lazily): nvs_get_str() disables the SPI flash
    // cache which on ESP32-S3 also disables PSRAM. Must run on an internal-RAM
    // thread — see AppContext::warmStorage().
    //
    // forceExternal=true skips the NVS lookup and always routes to /sd/data/.
    // Mandatory for WASM/JS tasks (PSRAM stack): LittleFS reads also disable the
    // SPI cache, causing the same stack-inaccessible crash.
    if (critical_) {
        base_ = "/system/data/" + bundleId_;
    } else if (forceExternal) {
        base_ = "/sd/data/" + bundleId_;
    } else {
        std::string loc;
        cfg_.getString("stor", storNsKey(bundleId_).c_str(), loc);
        base_ = (loc == "ext") ? "/sd/data/" + bundleId_ : "/system/data/" + bundleId_;
    }
}

std::string AppStorage::resolvePath(const char* name) const {
    if (!name || name[0] == '\0') return base_;
    return base_ + "/" + name;
}

// Create all path components that don't exist yet (recursive mkdir).
static void mkdirAll(IFileSystem* vfs, const std::string& path) {
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i] == '/') vfs->mkdir(path.substr(0, i));
    }
    vfs->mkdir(path);
}

bool AppStorage::write(const char* name, const uint8_t* data, size_t len) {
    if (!vfs_) return false;
    std::string path = resolvePath(name);
    auto slash = path.rfind('/');
    if (slash != std::string::npos) {
        std::string dir = path.substr(0, slash);
        std::vector<FsEntry> probe;
        if (!vfs_->list(dir, probe))
            mkdirAll(vfs_, dir);
    }
    return vfs_->write(path, data, len);
}

bool AppStorage::read(const char* name, std::vector<uint8_t>& out) {
    if (!vfs_) return false;
    return vfs_->read(resolvePath(name), out);
}

bool AppStorage::remove(const char* name) {
    if (!vfs_) return false;
    return vfs_->remove(resolvePath(name));
}

bool AppStorage::exists(const char* name) {
    if (!vfs_) return false;
    std::vector<uint8_t> probe;
    // Check by trying to read a zero-length probe; list parent dir as fallback
    std::string path = resolvePath(name);
    auto slash = path.rfind('/');
    if (slash == std::string::npos) return false;
    std::string dir  = path.substr(0, slash);
    std::string base = path.substr(slash + 1);
    std::vector<FsEntry> entries;
    if (!vfs_->list(dir, entries)) return false;
    for (auto& e : entries)
        if (e.name == base) return true;
    return false;
}

std::vector<std::string> AppStorage::list(const char* subdir) {
    std::vector<std::string> result;
    if (!vfs_) return result;
    std::string dir = resolvePath(subdir && subdir[0] ? subdir : "");
    std::vector<FsEntry> entries;
    if (!vfs_->list(dir, entries)) return result;
    for (auto& e : entries)
        if (!e.isDir) result.push_back(e.name);
    return result;
}

size_t AppStorage::usedBytes() const {
    if (!vfs_) return 0;
    size_t total = 0;
    // Scan both internal and external regardless of routing preference
    for (const char* base : {"/system/data/", "/sd/data/"}) {
        std::string dir = std::string(base) + bundleId_;
        std::vector<FsEntry> entries;
        if (!vfs_->list(dir, entries)) continue;
        for (auto& e : entries)
            if (!e.isDir) total += e.size;
    }
    return total;
}

AppStorage AppStorage::critical() const {
    return AppStorage(bundleId_, vfs_, cfg_, true);
}

} // namespace nema
