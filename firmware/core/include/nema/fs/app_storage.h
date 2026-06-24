#pragma once
#include "nema/hal/filesystem.h"
#include "nema/config/config_store.h"
#include <string>
#include <vector>
#include <cstdint>

// Plan 83 — Namespaced file storage for a single app.
// All paths are relative to the app's data directory and routed automatically
// to internal flash (/data/<id>/) or SD card (/sd/data/<id>/) based on the
// user's storage preference (stored in IConfigStore by StorageService).
//
// Obtain via AppContext::storage() — never construct directly in app code.

namespace nema {

class AppStorage {
public:
    // forceExternal: skip NVS lookup and always route to /sd/data/<id>/.
    // Required for tasks with PSRAM stacks (JS/WASM): LittleFS direct-reads
    // disable the SPI flash cache which also makes PSRAM inaccessible → crash.
    // SD-card reads go through a separate SPI controller and are always safe.
    AppStorage(std::string bundleId, IFileSystem* vfs,
               IConfigStore& cfg, bool critical = false,
               bool forceExternal = false);

    // File ops — name is a relative filename ("config.json", "cache/data.bin").
    // Routed to the correct physical path transparently.
    bool write(const char* name, const uint8_t* data, size_t len);
    bool read (const char* name, std::vector<uint8_t>& out);
    bool remove(const char* name);
    bool exists(const char* name);
    std::vector<std::string> list(const char* subdir = nullptr);

    // Total bytes used by this app's data directory (scans both internal + SD).
    size_t usedBytes() const;

    // Returns a view into the same app's data that always routes to internal
    // flash, regardless of the user's storage preference. Use for credentials,
    // private keys, or anything that must never touch an SD card.
    AppStorage critical() const;

private:
    std::string resolvePath(const char* name) const;

    std::string   bundleId_;
    IFileSystem*  vfs_;
    IConfigStore& cfg_;
    bool          critical_;
    // Pre-resolved in the constructor (NVS read happens there, not on every I/O
    // call). Must be constructed on a thread with an internal-RAM stack — PSRAM
    // threads cannot call nvs_get_str() because NVS disables the SPI cache which
    // also makes PSRAM inaccessible. See AppContext::warmStorage().
    std::string   base_;
};

} // namespace nema
