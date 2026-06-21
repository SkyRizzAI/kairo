// Plan 83 — StorageService implementation.
#include "nema/services/storage_service.h"
#include "nema/app/app_registry.h"
#include "nema/runtime.h"
#include <cstdio>
#include <cstring>

namespace nema {

// djb2 hash → 8-char hex — same algorithm as app_storage.cpp and nema_host_impl.cpp.
std::string StorageService::nsKey(const std::string& bundleId) {
    uint32_t h = 5381;
    for (char c : bundleId) h = ((h << 5) + h) + (uint8_t)c;
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", (unsigned int)h);
    return buf;
}

void StorageService::init(Runtime& rt) {
    rt_  = &rt;
    vfs_ = rt.fs();
    cfg_ = &rt.config();
}

std::string StorageService::resolveDataDir(const char* bundleId, bool critical) const {
    if (critical) return std::string("/system/data/") + bundleId;
    std::string loc;
    if (cfg_) cfg_->getString("stor", nsKey(bundleId).c_str(), loc);
    if (loc == "ext" && vfs_) {
        // Verify SD is actually mounted before routing there
        std::vector<FsEntry> probe;
        if (vfs_->list("/sd/data", probe) || vfs_->list("/sd", probe))
            return std::string("/sd/data/") + bundleId;
    }
    return std::string("/system/data/") + bundleId;
}

StorageLocation StorageService::locationOf(const char* bundleId) const {
    if (!cfg_) return StorageLocation::Internal;
    std::string loc;
    if (!cfg_->getString("stor", nsKey(bundleId).c_str(), loc))
        return StorageLocation::Internal;
    return loc == "ext" ? StorageLocation::External : StorageLocation::Internal;
}

void StorageService::setLocation(const char* bundleId, StorageLocation loc) {
    if (!cfg_) return;
    const char* val = (loc == StorageLocation::External) ? "ext" : "int";
    cfg_->setString("stor", nsKey(bundleId).c_str(), val);
}

bool StorageService::move(const char* bundleId, StorageLocation to) {
    if (!vfs_ || !cfg_) return false;

    std::string src = (to == StorageLocation::External)
        ? "/system/data/"    + std::string(bundleId)
        : "/sd/data/" + std::string(bundleId);
    std::string dst = (to == StorageLocation::External)
        ? "/sd/data/" + std::string(bundleId)
        : "/system/data/"    + std::string(bundleId);

    std::vector<FsEntry> entries;
    if (!vfs_->list(src, entries)) {
        // Source doesn't exist — just update the routing config
        setLocation(bundleId, to);
        return true;
    }

    // mkdir each component so /sd/system/data/<id> works even on fresh SD card.
    for (size_t i = 1; i < dst.size(); ++i)
        if (dst[i] == '/') vfs_->mkdir(dst.substr(0, i));
    vfs_->mkdir(dst);
    for (auto& e : entries) {
        if (e.isDir) continue;
        std::vector<uint8_t> buf;
        std::string srcPath = src + "/" + e.name;
        std::string dstPath = dst + "/" + e.name;
        if (!vfs_->read(srcPath, buf)) return false;
        if (!vfs_->write(dstPath, buf.data(), buf.size())) return false;
    }

    // Config update first (if we crash now, files exist in both places — harmless)
    setLocation(bundleId, to);

    // Remove source files (best-effort; leftover files cause no corruption)
    for (auto& e : entries)
        if (!e.isDir) vfs_->remove(src + "/" + e.name);

    return true;
}

size_t StorageService::scanDir(const std::string& path) const {
    if (!vfs_) return 0;
    size_t total = 0;
    std::vector<FsEntry> entries;
    if (!vfs_->list(path, entries)) return 0;
    for (auto& e : entries)
        if (!e.isDir) total += e.size;
    return total;
}

std::vector<StorageService::AppStorageInfo> StorageService::allApps() const {
    std::vector<AppStorageInfo> result;
    if (!rt_) return result;

    for (auto& m : rt_->apps().list()) {
        AppStorageInfo info;
        info.bundleId      = m.id   ? m.id   : "";
        info.displayName   = m.name ? m.name : info.bundleId;
        info.location      = locationOf(info.bundleId.c_str());
        info.internalBytes = scanDir("/system/data/"    + info.bundleId);
        info.externalBytes = scanDir("/sd/data/" + info.bundleId);
        info.movable       = m.storageMovable;
        info.hasCritical   = m.hasCriticalData;
        result.push_back(std::move(info));
    }
    return result;
}

StorageService::VolumeInfo StorageService::internalVolume() const {
    // LittleFS doesn't expose used/free bytes via IFileSystem yet (Plan 38).
    // Return what we can scan from /data/.
    VolumeInfo v;
    if (!vfs_) return v;
    std::vector<FsEntry> entries;
    if (!vfs_->list("/system/data", entries)) return v;
    for (auto& e : entries)
        if (e.isDir) v.usedBytes += scanDir("/system/data/" + e.name);
    return v;
}

StorageService::VolumeInfo StorageService::externalVolume() const {
    VolumeInfo v;
    if (!vfs_) return v;
    std::vector<FsEntry> entries;
    if (!vfs_->list("/sd/data", entries)) return v;
    for (auto& e : entries)
        if (e.isDir) v.usedBytes += scanDir("/sd/data/" + e.name);
    return v;
}

bool StorageService::hasExternal() const {
    if (!vfs_) return false;
    std::vector<FsEntry> probe;
    return vfs_->list("/sd", probe);
}

} // namespace nema
