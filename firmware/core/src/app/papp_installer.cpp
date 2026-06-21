// Plan 59 — .papp folder-based app installer (macOS .app style).
// A .papp is a DIRECTORY containing manifest.json + entry point + assets.
// Portable: copy the folder anywhere under /apps/ or /sd/apps/.
//
// Cache: tracks installed app IDs. On scan, only processes new/removed apps.
// Hot-reload: detects folder additions, deletions, and modifications.

#include "nema/app/papp_installer.h"
#include "nema/app/papp_package.h"
#include "nema/apps/js_app_store.h"
#include "nema/app/app_registry.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/hal/filesystem.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_set>
#include <cstring>

namespace nema {

static constexpr int kRootCount = 2;
static const char* kAppRoots[kRootCount] = { "/apps", "/sd/apps" };

// ── Cache ─────────────────────────────────────────────────────────────────

// Tracks which app IDs are currently installed (from .papp folders).
// When scanning, we compare discovered IDs against this set to detect
// new apps (install), missing apps (uninstall), and same apps (skip).
static std::unordered_set<std::string> s_installedIds;

// ── Helpers ───────────────────────────────────────────────────────────────

static bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Recursively collect all .papp entries (directories and files).
struct FoundPapp {
    std::string path;
    bool        isDir;
};
static void scanPappDirs(IFileSystem* fs, const std::string& dir,
                         std::vector<FoundPapp>& out) {
    std::vector<FsEntry> entries;
    if (!fs->list(dir, entries)) return;
    for (auto& e : entries) {
        std::string full = dir + "/" + e.name;
        if (e.isDir) {
            if (endsWith(e.name, ".papp")) {
                out.push_back({full, true});  // .papp folder
            } else {
                scanPappDirs(fs, full, out);  // recurse
            }
        } else if (endsWith(e.name, ".papp")) {
            out.push_back({full, false});  // single-file .papp
        }
    }
}

// Read app id from a .papp folder's manifest.json (or from binary header).
static std::string readAppId(Runtime& rt, IFileSystem* fs,
                             const FoundPapp& entry) {
    if (entry.isDir) {
        std::vector<uint8_t> mData;
        if (!fs->read(entry.path + "/manifest.json", mData)) return "";
        std::string mStr(mData.begin(), mData.end());
        auto mj = nlohmann::json::parse(mStr, nullptr, false);
        if (mj.is_discarded()) return "";
        return mj.value("id", "");
    } else {
        // Binary file: parse header for id (quick check)
        std::vector<uint8_t> data;
        if (!fs->read(entry.path, data)) return "";
        std::string s(data.begin(), data.end());
        auto idPos = s.find("\"id\"");
        if (idPos == std::string::npos) return "";
        auto colon = s.find(':', idPos);
        if (colon == std::string::npos) return "";
        auto q1 = s.find('"', colon + 1);
        if (q1 == std::string::npos) return "";
        auto q2 = s.find('"', q1 + 1);
        if (q2 == std::string::npos) return "";
        return s.substr(q1 + 1, q2 - q1 - 1);
    }
}

// Install from a .papp DIRECTORY.
static bool installFromDir(Runtime& rt, IFileSystem* fs,
                           const std::string& dir) {
    std::vector<uint8_t> mData;
    if (!fs->read(dir + "/manifest.json", mData)) return false;
    std::string mStr(mData.begin(), mData.end());

    auto mj = nlohmann::json::parse(mStr, nullptr, false);
    if (mj.is_discarded()) return false;

    std::string id      = mj.value("id", "");
    std::string name    = mj.value("name", "");
    std::string version = mj.value("version", "1.0.0");
    std::string dsrv    = mj.value("display_server", "");
    std::string rtStr   = mj.value("runtime", "js");

    if (id.empty()) return false;
    if (name.empty()) name = id;

    std::string entryPath;
    for (const char* c : {"app.js", "app.wasm", "main.js"}) {
        std::vector<uint8_t> d;
        if (fs->read(dir + "/" + c, d)) { entryPath = c; break; }
    }
    if (entryPath.empty()) return false;

    std::vector<uint8_t> code;
    if (!fs->read(dir + "/" + entryPath, code)) return false;
    std::string js(code.begin(), code.end());

    rt.log().info("PappInstaller", "install", {{"id", id}, {"name", name}});
    return JsAppStore::instance().installApp(rt, id, name, version, js, dsrv);
}

// Install from a single-file .papp FILE.
static bool installFromFile(Runtime& rt, IFileSystem* fs,
                            const std::string& path) {
    std::vector<uint8_t> data;
    if (!fs->read(path, data)) return false;
    return installPapp(rt, data.data(), data.size());
}

// ── Public API ─────────────────────────────────────────────────────────────

bool installPapp(Runtime& rt, const uint8_t* data, size_t len) {
    PappPackage pkg = parsePapp(data, len);
    if (!pkg.valid) return false;
    if (pkg.singleFile) {
        return JsAppStore::instance().installPappBytes(
            rt, reinterpret_cast<const char*>(data), len);
    }
    return installPappBundle(rt, pkg);
}

void loadInstalledPapps(Runtime& rt) {
    IFileSystem* fs = rt.fs();
    if (!fs) return;

    // Phase 1: scan all .papp entries and collect discovered IDs
    std::unordered_set<std::string> discovered;
    std::vector<FoundPapp> entries;
    for (int i = 0; i < kRootCount; i++) {
        scanPappDirs(fs, kAppRoots[i], entries);
    }

    for (auto& e : entries) {
        std::string id = readAppId(rt, fs, e);
        if (!id.empty()) discovered.insert(id);
    }

    // Phase 2: install new apps (discovered but not cached)
    int installed = 0;
    for (auto& e : entries) {
        std::string id = readAppId(rt, fs, e);
        if (id.empty() || s_installedIds.count(id)) continue;  // already cached

        bool ok = e.isDir ? installFromDir(rt, fs, e.path)
                          : installFromFile(rt, fs, e.path);
        if (ok) {
            s_installedIds.insert(id);
            installed++;
        }
    }

    // Phase 3: uninstall removed apps (cached but not discovered)
    std::vector<std::string> removed;
    for (const auto& id : s_installedIds) {
        if (!discovered.count(id)) removed.push_back(id);
    }
    for (const auto& id : removed) {
        rt.apps().uninstall(id.c_str());
        s_installedIds.erase(id);
        rt.log().info("PappInstaller", "uninstall", {{"id", id}});
    }

    if (installed > 0 || !removed.empty()) {
        rt.log().info("PappInstaller", "scan done",
                      {{"installed", std::to_string(installed)},
                       {"removed", std::to_string(removed.size())},
                       {"total", std::to_string(s_installedIds.size())}});
    }
}

bool installPappBundle(Runtime& rt, const PappPackage& pkg) {
    IFileSystem* fs = rt.fs();
    if (!fs || pkg.entryCount == 0) return false;

    const PappEntry& mEntry = pkg.entries[0];
    std::string manifestStr(reinterpret_cast<const char*>(mEntry.data), mEntry.stored);
    auto mj = nlohmann::json::parse(manifestStr, nullptr, false);
    if (mj.is_discarded()) return false;

    std::string id = mj.value("id", "");
    if (id.empty()) return false;

    std::string dir = std::string("/apps/") + id + ".papp";
    fs->mkdir(dir);

    for (size_t i = 0; i < pkg.entryCount; ++i) {
        const PappEntry& e = pkg.entries[i];
        std::string entryName(e.name, e.nameLen);
        fs->write(dir + "/" + entryName, e.data, e.stored);
    }

    s_installedIds.erase(id);  // invalidate cache for this id
    loadInstalledPapps(rt);    // will pick up the new folder
    return true;
}

} // namespace nema
