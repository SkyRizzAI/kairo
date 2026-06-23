#pragma once
#include "nema/service.h"
#include "nema/fs/app_storage.h"
#include <string>
#include <vector>
#include <cstdint>

// Plan 83 — StorageService: central routing + management for app data storage.
//
// Tracks per-app storage location preference (internal vs SD) in IConfigStore.
// Provides move operations (copy all files → update config → delete source) and
// volume usage stats for the Storage Settings screen.
//
// Register in platform init:
//   rt.container().registerAs<StorageService>(&storageSvc_);
//   rt.services().add(storageSvc_);

namespace nema {

class Runtime;
class AppRegistry;
struct IFileSystem;
class IConfigStore;

enum class StorageLocation { Internal, External, Auto };

class StorageService : public IService {
public:
    void init(Runtime& rt);

    // ── Routing ──────────────────────────────────────────────────────────────

    // Resolve the base data directory for a bundle.
    // critical=true always returns the internal path.
    std::string resolveDataDir(const char* bundleId, bool critical = false) const;

    // Current explicit location preference for a bundle (does not fall back).
    StorageLocation locationOf(const char* bundleId) const;

    // Set the routing preference (persisted in config store).
    void setLocation(const char* bundleId, StorageLocation loc);

    // ── Management ───────────────────────────────────────────────────────────

    struct VolumeInfo {
        size_t totalBytes = 0;
        size_t usedBytes  = 0;
        size_t freeBytes  = 0;
    };

    struct AppStorageInfo {
        std::string     bundleId;
        std::string     displayName;
        StorageLocation location;       // resolved preference
        size_t          internalBytes;
        size_t          externalBytes;
        bool            movable;        // from AppManifest::storageMovable
        bool            hasCritical;    // from AppManifest::hasCriticalData
    };

    // Enumerate all registered apps with their storage info (scans on call).
    std::vector<AppStorageInfo> allApps() const;

    // Move all of a bundle's data to the target location.
    // Updates routing config after successful copy; leaves source intact on failure.
    bool move(const char* bundleId, StorageLocation to);

    // Volume stats (on-demand; may be slow — call from a worker thread).
    VolumeInfo internalVolume() const;
    VolumeInfo externalVolume() const;
    bool       hasExternal()   const;

    // SD card info: mounted state + capacity from the FAT filesystem.
    // totalBytes / freeBytes are 0 on backends that don't support statvfs.
    struct SdCardInfo {
        bool     mounted    = false;
        uint64_t totalBytes = 0;
        uint64_t freeBytes  = 0;
    };
    SdCardInfo sdCardInfo() const;

    // Software-eject the SD card: flushes streams and removes it from the VFS
    // router so subsequent file operations fail cleanly. The user can then
    // physically remove the card. Returns false if no SD is mounted.
    bool ejectSd();

    // ── IService ─────────────────────────────────────────────────────────────

    const char* name() const override { return "StorageService"; }
    void start() override {}
    void stop()  override {}
    void tick(uint64_t) override {}

private:
    static std::string nsKey(const std::string& bundleId);
    size_t scanDir(const std::string& path) const;

    Runtime*      rt_  = nullptr;
    IFileSystem*  vfs_ = nullptr;
    IConfigStore* cfg_ = nullptr;
};

} // namespace nema
