// Plan 71 — Asset loader implementations.
// BitmapAsset, drawFile, AnimMeta, AnimAsset, AssetPackLoader, AssetPackRegistry.
#include "nema/ui/asset_loader.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace nema::asset {

// ── helpers ─────────────────────────────────────────────────────────────────

namespace {

static bool readFile(IFileSystem& fs, const char* path, std::vector<uint8_t>& out) {
    std::string spath(path);
    return fs.read(spath, out) && !out.empty();
}

static std::string readTextFile(IFileSystem& fs, const char* path) {
    std::vector<uint8_t> raw;
    if (!readFile(fs, path, raw)) return {};
    return std::string(raw.begin(), raw.end());
}

static bool parseDimSuffix(const char* name, uint16_t& w, uint16_t& h) {
    const char* dot = std::strrchr(name, '.');
    const char* end = dot ? dot : name + std::strlen(name);

    const char* x = nullptr;
    for (const char* p = end - 1; p > name; --p) {
        if (*p == 'x' || *p == 'X') { x = p; break; }
    }
    if (!x) return false;

    const char* underscore = nullptr;
    for (const char* p = x - 1; p >= name; --p) {
        if (*p == '_') { underscore = p; break; }
    }
    if (!underscore) return false;

    int iw = 0, ih = 0;
    if (std::sscanf(underscore, "_%dx%d", &iw, &ih) != 2 &&
        std::sscanf(underscore, "_%dX%d", &iw, &ih) != 2)
        return false;

    if (iw <= 0 || ih <= 0 || iw > 65535 || ih > 65535) return false;
    w = (uint16_t)iw;
    h = (uint16_t)ih;
    return true;
}

} // namespace

// ── BitmapAsset ─────────────────────────────────────────────────────────────

bool BitmapAsset::parseDimFromName(const char* path, uint16_t& w, uint16_t& h) {
    const char* slash = std::strrchr(path, '/');
    const char* name  = slash ? slash + 1 : path;
    return parseDimSuffix(name, w, h);
}

bool BitmapAsset::load(IFileSystem& fs, const char* path) {
    uint16_t w = 0, h = 0;
    if (!parseDimFromName(path, w, h)) return false;
    return load(fs, path, w, h);
}

bool BitmapAsset::load(IFileSystem& fs, const char* path, uint16_t w, uint16_t h) {
    release();

    std::vector<uint8_t> raw;
    if (!readFile(fs, path, raw)) return false;

    size_t expected = (size_t)((uint32_t)w * h + 7) / 8;
    if (raw.size() < expected) return false;

    width  = w;
    height = h;
    data   = std::move(raw);
    return true;
}

void BitmapAsset::release() {
    data.clear();
    width  = 0;
    height = 0;
}

// ── drawFile ────────────────────────────────────────────────────────────────

bool drawFile(Canvas& c, IFileSystem& fs, const char* path,
              uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    std::vector<uint8_t> buf;
    std::string spath(path);
    if (!fs.read(spath, buf)) return false;

    size_t expected = (size_t)((uint32_t)w * h + 7) / 8;
    if (buf.size() < expected) return false;

    c.drawBitmap(x, y, w, h, buf.data());
    return true;
}

// ── AnimMeta ────────────────────────────────────────────────────────────────

AnimMeta parseAnimMeta(const char* txt) {
    AnimMeta m;
    if (!txt || !*txt) return m;

    const char* p = txt;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
        if (!*p) break;

        const char* keyStart = p;
        while (*p && *p != ':' && *p != '\n') ++p;
        const char* keyEnd = p;
        if (*p == ':') ++p;

        while (*p && (*p == ' ' || *p == '\t')) ++p;

        const char* valStart = p;
        while (*p && *p != '\r' && *p != '\n') ++p;
        const char* valEnd = p;
        while (*p == '\r' || *p == '\n') ++p;

        std::string key(keyStart, keyEnd - keyStart);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.pop_back();

        std::string val(valStart, valEnd - valStart);
        if (val.empty()) continue;

        int iv = std::stoi(val);

        if (key == "Width")           m.width      = (uint16_t)iv;
        else if (key == "Height")     m.height     = (uint16_t)iv;
        else if (key == "Passive frames") m.frameCount = (uint8_t)iv;
        else if (key == "Frame rate")      m.frameRate  = (uint8_t)iv;
        else if (key == "Active cycles" || key == "Duration") {
            if (iv > 0) m.loop = true;
        }
    }
    return m;
}

// ── AnimAsset ───────────────────────────────────────────────────────────────

bool AnimAsset::load(IFileSystem& fs, const char* dirPath) {
    release();

    std::string dir(dirPath);
    if (dir.back() != '/') dir += '/';
    std::string metaPath = dir + "meta.txt";

    std::string metaTxt = readTextFile(fs, metaPath.c_str());
    if (metaTxt.empty()) return false;

    AnimMeta meta = parseAnimMeta(metaTxt.c_str());
    if (meta.width == 0 || meta.height == 0 || meta.frameCount == 0)
        return false;

    size_t frameBytes = (size_t)((uint32_t)meta.width * meta.height + 7) / 8;

    for (uint8_t i = 0; i < meta.frameCount; ++i) {
        char fname[64];
        std::snprintf(fname, sizeof(fname), "%sframe_%u.bm",
                      dir.c_str(), (unsigned)i);

        std::vector<uint8_t> buf;
        if (!readFile(fs, fname, buf) || buf.size() < frameBytes) {
            release();
            return false;
        }

        buffers.push_back(std::move(buf));
        frames.push_back({buffers.back().data(), meta.width, meta.height});
    }

    def.frames     = frames.data();
    def.frameCount = meta.frameCount;
    def.frameRate  = meta.frameRate;
    def.loop       = meta.loop;
    return true;
}

void AnimAsset::release() {
    buffers.clear();
    frames.clear();
    def = {};
}

// ── AssetPackLoader ─────────────────────────────────────────────────────────

AssetPackLoader::AssetPackLoader(IFileSystem& fs, const char* packPath)
    : fs_(fs), basePath_(packPath) {
    if (basePath_.back() != '/') basePath_ += '/';
}

BitmapAsset AssetPackLoader::loadIcon(const char* name) {
    std::string full = basePath_ + "icons/" + name;
    BitmapAsset a;
    a.load(fs_, full.c_str());
    return a;
}

AnimAsset AssetPackLoader::loadAnimation(const char* name) {
    std::string full = basePath_ + "animations/" + name;
    AnimAsset a;
    a.load(fs_, full.c_str());
    return a;
}

std::vector<std::string> AssetPackLoader::listIcons() {
    std::vector<FsEntry> entries;
    std::vector<std::string> result;
    std::string path = basePath_ + "icons";
    if (!fs_.list(path, entries)) return result;
    for (auto& e : entries)
        if (!e.isDir) result.push_back(e.name);
    return result;
}

std::vector<std::string> AssetPackLoader::listAnimations() {
    std::vector<FsEntry> entries;
    std::vector<std::string> result;
    std::string path = basePath_ + "animations";
    if (!fs_.list(path, entries)) return result;
    for (auto& e : entries)
        if (e.isDir) result.push_back(e.name);
    return result;
}

bool AssetPackLoader::isValid() const {
    std::vector<FsEntry> entries;
    return fs_.list(basePath_, entries);
}

// ── AssetPackRegistry ───────────────────────────────────────────────────────

AssetPackRegistry& AssetPackRegistry::instance() {
    static AssetPackRegistry inst;
    return inst;
}

void AssetPackRegistry::registerIcon(const char* handle, const BitmapAsset& asset) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->handle == handle) {
            it->asset = &asset;
            return;
        }
    }
    entries_.push_back({std::string(handle), &asset, {}});
}

const BitmapAsset* AssetPackRegistry::find(const char* handle) const {
    if (!handle) return nullptr;
    for (auto& e : entries_)
        if (e.handle == handle) return e.asset;
    return nullptr;
}

const aether::ui::IconDef* AssetPackRegistry::findIconDef(const char* handle) const {
    if (!handle) return nullptr;
    for (auto& e : entries_) {
        if (e.handle == handle && e.asset && e.asset->valid()) {
            // Reconstruct IconDef from live data — avoids stale c_str() pointer.
            e.def = { e.handle.c_str(), e.asset->bits(),
                      (uint8_t)e.asset->width, (uint8_t)e.asset->height };
            return &e.def;
        }
    }
    return nullptr;
}

void AssetPackRegistry::clear() {
    entries_.clear();
}

// ── seedDemoAssets ──────────────────────────────────────────────────────────
// Plan 71: Populate a filesystem with a minimal Flipper-compatible asset pack
// so the loader pipeline can be exercised without an SD card full of .bm files.
void seedDemoAssets(IFileSystem& fs) {
    // Ensure directory structure
    fs.mkdir("/packs");
    fs.mkdir("/packs/default");
    fs.mkdir("/packs/default/icons");
    fs.mkdir("/packs/default/animations");
    fs.mkdir("/packs/default/animations/spinner");

    // ── icons (8x8, 8 bytes each) ────────────────────────────────────────

    auto w8 = [&](const char* name, const uint8_t* d) {
        std::string path = std::string("/packs/default/icons/") + name;
        fs.write(path, d, 8);
    };

    // status.wifi
    static const uint8_t i_wifi[] = {0x00,0x3C,0x42,0x18,0x24,0x08,0x18,0x00};
    w8("wifi_8x8.bm", i_wifi);

    // status.bt
    static const uint8_t i_bt[] = {0x08,0x0C,0x2A,0x18,0x18,0x2A,0x0C,0x08};
    w8("bt_8x8.bm", i_bt);

    // status.battery
    static const uint8_t i_bat[] = {0x00,0x7E,0x42,0x5A,0x5A,0x42,0x7E,0x00};
    w8("battery_8x8.bm", i_bat);

    // feature.apps (grid)
    static const uint8_t i_apps[] = {0xDB,0xDB,0x00,0xDB,0xDB,0x00,0xDB,0xDB};
    w8("apps_8x8.bm", i_apps);

    // feature.settings (gear)
    static const uint8_t i_sett[] = {0x18,0x3C,0x66,0xFF,0xFF,0x66,0x3C,0x18};
    w8("settings_8x8.bm", i_sett);

    // action.warning (triangle)
    static const uint8_t i_warn[] = {0x10,0x38,0x6C,0xC6,0xFE,0x00,0x38,0x00};
    w8("warning_8x8.bm", i_warn);

    // action.info
    static const uint8_t i_info[] = {0x18,0x18,0x00,0x38,0x18,0x18,0x3C,0x00};
    w8("info_8x8.bm", i_info);

    // ── spinner animation (4 frames, 8x8) ────────────────────────────────

    auto wf = [&](const char* name, const uint8_t* d) {
        std::string path = std::string("/packs/default/animations/spinner/") + name;
        fs.write(path, d, 8);
    };

    static const uint8_t s0[] = {0xF8,0x88,0x88,0x88,0xF8,0x00,0x00,0x00};
    wf("frame_0.bm", s0);
    static const uint8_t s1[] = {0x00,0x00,0x00,0xF8,0x88,0x88,0x88,0xF8};
    wf("frame_1.bm", s1);
    static const uint8_t s2[] = {0x1F,0x11,0x11,0x11,0x1F,0x00,0x00,0x00};
    wf("frame_2.bm", s2);
    static const uint8_t s3[] = {0x00,0x00,0x00,0x1F,0x11,0x11,0x11,0x1F};
    wf("frame_3.bm", s3);

    // meta.txt for spinner
    const char* mt1 = "Width: 8\nHeight: 8\nPassive frames: 4\nFrame rate: 4\n";
    fs.write(std::string("/packs/default/animations/spinner/meta.txt"),
             (const uint8_t*)mt1, std::strlen(mt1));
}

// ── PanimAsset ───────────────────────────────────────────────────────────────

static inline uint16_t read16le(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

bool PanimAsset::load(IFileSystem& fs, const char* path) {
    release();

    std::string spath(path);
    if (!fs.read(spath, rawData) || rawData.size() < 14) return false;

    const uint8_t* d = rawData.data();
    // Magic "PANM"
    if (d[0]!='P' || d[1]!='A' || d[2]!='N' || d[3]!='M') return false;
    // Version
    if (d[4] != 1) return false;

    w            = read16le(d + 5);
    h            = read16le(d + 7);
    fps          = d[9];
    passiveCount = d[10];
    activeCount  = d[11];
    uint8_t uniqueCount    = d[12];
    uint8_t framesOrderLen = d[13];

    if (w == 0 || h == 0 || uniqueCount == 0) return false;

    size_t off = 14;
    // Frames order table
    if (off + framesOrderLen > rawData.size()) return false;
    if (framesOrderLen > 0) {
        framesOrder.assign(d + off, d + off + framesOrderLen);
        off += framesOrderLen;
    }

    // Unique frame bitmaps — each is ceil(w/8)*h bytes
    size_t bytesPerFrame = ((size_t)((w + 7) / 8)) * h;
    size_t totalBitmap   = (size_t)uniqueCount * bytesPerFrame;
    if (off + totalBitmap > rawData.size()) return false;

    frames.resize(uniqueCount);
    for (uint8_t i = 0; i < uniqueCount; i++) {
        frames[i].bitmap = rawData.data() + off + i * bytesPerFrame;
        frames[i].width  = w;
        frames[i].height = h;
    }

    def.frames         = frames.data();
    def.frameCount     = uniqueCount;
    def.frameRate      = fps;
    def.loop           = true;
    def.framesOrder    = framesOrder.empty() ? nullptr : framesOrder.data();
    def.framesOrderLen = framesOrderLen;
    def.passiveCount   = passiveCount;
    def.activeCount    = activeCount;
    return true;
}

void PanimAsset::release() {
    rawData.clear();
    framesOrder.clear();
    frames.clear();
    def = {};
    w = h = 0; fps = passiveCount = activeCount = 0;
}

} // namespace nema::asset
