#include "kairo/fs/vfs.h"

namespace kairo {

namespace {

std::string norm(const std::string& in) {
    std::string s = in;
    if (s.empty() || s[0] != '/') s = "/" + s;
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '/' && !out.empty() && out.back() == '/') continue;
        out.push_back(c);
    }
    if (out.size() > 1 && out.back() == '/') out.pop_back();
    return out;
}

std::string parentOf(const std::string& path) {
    auto slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) return "/";
    return path.substr(0, slash);
}

std::string baseOf(const std::string& path) {
    auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Is `path` inside the mount point `mp` (or equal to it)?
bool under(const std::string& path, const std::string& mp) {
    if (mp == "/") return true;
    if (path == mp) return true;
    return path.size() > mp.size() && path.compare(0, mp.size(), mp) == 0 && path[mp.size()] == '/';
}

} // namespace

void Vfs::mount(const std::string& mountPoint, IFileSystem* fs) {
    if (!fs) return;
    std::string mp = norm(mountPoint);
    unmount(mp);
    mounts_.push_back({mp, fs});
    // Longest mount point first so resolve() picks the most specific backend.
    for (size_t i = mounts_.size() - 1; i > 0; --i) {
        if (mounts_[i].point.size() > mounts_[i - 1].point.size())
            std::swap(mounts_[i], mounts_[i - 1]);
        else
            break;
    }
}

void Vfs::unmount(const std::string& mountPoint) {
    std::string mp = norm(mountPoint);
    for (auto it = mounts_.begin(); it != mounts_.end(); ++it)
        if (it->point == mp) { mounts_.erase(it); return; }
}

bool Vfs::isMountPoint(const std::string& path) const {
    std::string p = norm(path);
    for (const auto& m : mounts_) if (m.point == p) return true;
    return false;
}

Vfs::Mount* Vfs::resolve(const std::string& path, std::string& sub) {
    for (auto& m : mounts_) {                 // longest-first → most specific wins
        if (under(path, m.point)) {
            if (m.point == "/") sub = path;
            else {
                sub = path.substr(m.point.size());
                if (sub.empty()) sub = "/";
            }
            return &m;
        }
    }
    return nullptr;
}

bool Vfs::list(const std::string& path, std::vector<FsEntry>& out) {
    std::string p = norm(path);
    std::string sub;
    Mount* m = resolve(p, sub);
    bool ok = m ? m->fs->list(sub, out) : false;

    // Synthesize entries for mount points that live directly under `p`
    // (e.g. listing "/" shows "sd" when "/sd" is mounted on another backend).
    for (const auto& mnt : mounts_) {
        if (mnt.point == "/" || parentOf(mnt.point) != p) continue;
        std::string base = baseOf(mnt.point);
        bool exists = false;
        for (const auto& e : out) if (e.name == base) { exists = true; break; }
        if (!exists) out.push_back({base, true, 0});
        ok = true;   // a dir that only holds mount points is still listable
    }
    return ok;
}

bool Vfs::read(const std::string& path, std::vector<uint8_t>& out) {
    std::string sub;
    Mount* m = resolve(norm(path), sub);
    return m && m->fs->read(sub, out);
}

bool Vfs::write(const std::string& path, const uint8_t* data, size_t len) {
    std::string sub;
    Mount* m = resolve(norm(path), sub);
    return m && m->fs->write(sub, data, len);
}

bool Vfs::mkdir(const std::string& path) {
    std::string p = norm(path);
    if (isMountPoint(p)) return false;        // a mount point already exists as a dir
    std::string sub;
    Mount* m = resolve(p, sub);
    return m && m->fs->mkdir(sub);
}

bool Vfs::remove(const std::string& path) {
    std::string p = norm(path);
    if (isMountPoint(p)) return false;        // unmount instead of deleting a mount
    std::string sub;
    Mount* m = resolve(p, sub);
    return m && m->fs->remove(sub);
}

} // namespace kairo
