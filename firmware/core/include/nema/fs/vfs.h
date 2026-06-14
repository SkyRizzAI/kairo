#pragma once
#include "nema/hal/filesystem.h"
#include <string>
#include <vector>

namespace nema {

// Vfs — a Linux-style virtual filesystem: ONE namespace rooted at "/", assembled
// from several backends mounted at paths. It IS an IFileSystem (composite), so the
// PLP File channel and the Forge browser see a single tree and never care which
// backend serves a path.
//
//   vfs.mount("/",   &flash);   // root / system partition
//   vfs.mount("/sd",  &sdcard);  // microSD, mounted when a card is present
//   vfs.mount("/tmp", &ram);     // scratch
//
// A path is routed to the backend with the LONGEST matching mount point, and the
// mount prefix is stripped before delegating ("/sd/log.txt" → sdcard "/log.txt").
// list() also synthesizes a directory entry for any mount point that is a direct
// child of the listed dir, so `ls /` shows `sd`/`tmp` even though the root backend
// has no such folders. Unmounting (e.g. SD removed) just drops the mount.
class Vfs : public IFileSystem {
public:
    void mount(const std::string& mountPoint, IFileSystem* fs);
    void unmount(const std::string& mountPoint);
    bool isMountPoint(const std::string& path) const;

    const char* name() const override { return "vfs"; }

    bool list  (const std::string& path, std::vector<FsEntry>& out) override;
    bool read  (const std::string& path, std::vector<uint8_t>& out) override;
    bool write (const std::string& path, const uint8_t* data, size_t len) override;
    bool mkdir (const std::string& path) override;
    bool remove(const std::string& path) override;

private:
    struct Mount { std::string point; IFileSystem* fs; };

    // Resolve a path to its backend + the path within that backend. Returns
    // {nullptr,...} if nothing is mounted (no root).
    Mount* resolve(const std::string& path, std::string& sub);

    std::vector<Mount> mounts_;   // kept sorted longest-point-first for resolution
};

} // namespace nema
