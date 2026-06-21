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
    snprintf(buf, sizeof(buf), "%08x", h);
    return buf;
}

AppStorage::AppStorage(std::string bundleId, IFileSystem* vfs,
                       IConfigStore& cfg, bool critical)
    : bundleId_(std::move(bundleId)), vfs_(vfs), cfg_(cfg), critical_(critical) {}

std::string AppStorage::resolvePath(const char* name) const {
    std::string base;
    if (critical_) {
        base = "/data/" + bundleId_;
    } else {
        std::string loc;
        cfg_.getString("stor", storNsKey(bundleId_).c_str(), loc);
        base = (loc == "ext") ? "/sd/data/" + bundleId_ : "/data/" + bundleId_;
    }
    if (!name || name[0] == '\0') return base;
    return base + "/" + name;
}

bool AppStorage::write(const char* name, const uint8_t* data, size_t len) {
    if (!vfs_) return false;
    std::string path = resolvePath(name);
    // Ensure parent dir exists
    auto slash = path.rfind('/');
    if (slash != std::string::npos) {
        std::string dir = path.substr(0, slash);
        vfs_->mkdir(dir);
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
    for (const char* base : {"/data/", "/sd/data/"}) {
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
