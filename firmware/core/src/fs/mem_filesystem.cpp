#include "nema/fs/mem_filesystem.h"

namespace nema {

namespace {

// Normalize to an absolute, slash-collapsed path with no trailing slash (root
// stays "/"). "a//b/" → "/a/b", "" → "/".
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

} // namespace

void MemFileSystem::ensureParents(const std::string& path) {
    std::string p = parentOf(path);
    while (true) {
        dirs_.insert(p);
        if (p == "/") break;
        p = parentOf(p);
    }
}

bool MemFileSystem::list(const std::string& path, std::vector<FsEntry>& out) {
    std::string dir = norm(path);
    if (!dirs_.count(dir)) return false;
    out.clear();
    for (const auto& d : dirs_)
        if (d != "/" && parentOf(d) == dir) out.push_back({baseOf(d), true, 0});
    for (const auto& f : files_)
        if (parentOf(f.first) == dir) out.push_back({baseOf(f.first), false, (uint32_t)f.second.size()});
    return true;
}

bool MemFileSystem::read(const std::string& path, std::vector<uint8_t>& out) {
    auto it = files_.find(norm(path));
    if (it == files_.end()) return false;
    out = it->second;
    return true;
}

bool MemFileSystem::write(const std::string& path, const uint8_t* data, size_t len) {
    std::string p = norm(path);
    if (p == "/" || dirs_.count(p)) return false;   // refuse to overwrite a directory
    ensureParents(p);
    files_[p].assign(data, data + len);
    return true;
}

bool MemFileSystem::mkdir(const std::string& path) {
    std::string p = norm(path);
    if (p == "/" || files_.count(p)) return false;
    ensureParents(p);
    dirs_.insert(p);
    return true;
}

bool MemFileSystem::remove(const std::string& path) {
    std::string p = norm(path);
    if (files_.erase(p)) return true;
    if (p == "/" || !dirs_.count(p)) return false;
    // Refuse to remove a non-empty directory.
    for (const auto& f : files_) if (parentOf(f.first) == p) return false;
    for (const auto& d : dirs_)  if (d != p && parentOf(d) == p) return false;
    dirs_.erase(p);
    return true;
}

bool MemFileSystem::rename(const std::string& src, const std::string& dst) {
    std::string s = norm(src), d = norm(dst);
    if (s == d || d.empty() || d == "/") return false;
    // File rename.
    auto it = files_.find(s);
    if (it != files_.end()) {
        if (dirs_.count(d)) return false;   // dst is an existing dir
        ensureParents(d);
        files_[d] = std::move(it->second);
        files_.erase(it);
        return true;
    }
    // Directory rename: rewrite every path under src.
    if (!dirs_.count(s)) return false;
    std::string srcPfx = s + "/";
    // Collect files to move.
    std::vector<std::pair<std::string, std::vector<uint8_t>>> fmove;
    for (auto& kv : files_)
        if (kv.first.substr(0, srcPfx.size()) == srcPfx)
            fmove.push_back({d + kv.first.substr(s.size()), std::move(kv.second)});
    for (auto& kv : fmove) files_.erase(s + kv.first.substr(d.size()));
    // Collect dirs to move.
    std::vector<std::string> dmove;
    for (const auto& dir : dirs_)
        if (dir.substr(0, srcPfx.size()) == srcPfx) dmove.push_back(dir);
    for (const auto& dir : dmove) dirs_.erase(dir);
    dirs_.erase(s);
    // Insert new paths.
    ensureParents(d);
    dirs_.insert(d);
    for (const auto& dir : dmove) dirs_.insert(d + dir.substr(s.size()));
    for (auto& kv : fmove) { ensureParents(kv.first); files_[kv.first] = std::move(kv.second); }
    return true;
}

void MemFileSystem::seed(const std::string& path, const std::string& text) {
    write(path, (const uint8_t*)text.data(), text.size());
}

} // namespace nema
