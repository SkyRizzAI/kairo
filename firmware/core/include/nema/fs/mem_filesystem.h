#pragma once
#include "nema/hal/filesystem.h"
#include <map>
#include <set>

namespace nema {

// MemFileSystem — in-RAM IFileSystem. Files live in a path→bytes map; directories
// are tracked explicitly (so empty dirs persist) and auto-created for any written
// file's ancestors. Volatile: nothing survives a reboot/page reload. It is the
// simulator's storage and a stopgap on hardware until LittleFS (Plan 38) replaces
// it with a persistent backend behind the same IFileSystem interface.
class MemFileSystem : public IFileSystem {
public:
    MemFileSystem() { dirs_.insert("/"); }

    const char* name() const override { return "MemFileSystem"; }

    bool list  (const std::string& path, std::vector<FsEntry>& out) override;
    bool read  (const std::string& path, std::vector<uint8_t>& out) override;
    bool write (const std::string& path, const uint8_t* data, size_t len) override;
    bool mkdir (const std::string& path) override;
    bool remove(const std::string& path) override;

    // Convenience for seeding demo content at boot.
    void seed(const std::string& path, const std::string& text);

private:
    void ensureParents(const std::string& path);

    std::map<std::string, std::vector<uint8_t>> files_;
    std::set<std::string>                        dirs_;
};

} // namespace nema
