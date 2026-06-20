// Plan 71 — Integration test for asset loading pipeline.
// Tests BitmapAsset, AnimAsset, AssetPackLoader, and AssetPackRegistry.
#include "nema/ui/asset_loader.h"
#include "nema/fs/mem_filesystem.h"
#include "nema/ui/icon_pack.h"
#include <cstdio>
#include <cstring>

using namespace nema;

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); g_fail++; } \
    else         { std::printf("  ok:   %s\n", msg); } \
} while (0)

// ── helpers ──────────────────────────────────────────────────────────────────

// Create a simple 8x8 .bm file with given byte pattern, write to MemFileSystem.
static void writeBm(MemFileSystem& fs, const char* path,
                    const uint8_t* data, size_t len) {
    fs.write(std::string(path), data, len);
}

// Write a meta.txt with animation metadata.
static void writeMetaTxt(MemFileSystem& fs, const char* dirPath,
                         uint16_t w, uint16_t h, uint8_t frames, uint8_t fps) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "Width: %u\nHeight: %u\nPassive frames: %u\nFrame rate: %u\n",
                  (unsigned)w, (unsigned)h, (unsigned)frames, (unsigned)fps);
    std::string path = std::string(dirPath) + "/meta.txt";
    fs.seed(path, buf);
}

// Tell MemFileSystem about a directory (list needs it).
static void ensureDir(MemFileSystem& fs, const char* path) {
    fs.mkdir(std::string(path));
}

// ── tests ────────────────────────────────────────────────────────────────────

static void test_dim_parsing() {
    std::printf("[BitmapAsset dimension parsing]\n");

    uint16_t w = 0, h = 0;
    CHECK(asset::BitmapAsset::parseDimFromName("icon_8x8.bm", w, h), "8x8");
    CHECK(w == 8 && h == 8, "  dimensions match");

    w = h = 0;
    CHECK(asset::BitmapAsset::parseDimFromName("/packs/MyPack/icons/wifi_20x15.bm", w, h), "path with 20x15");
    CHECK(w == 20 && h == 15, "  dimensions match");

    w = h = 0;
    CHECK(!asset::BitmapAsset::parseDimFromName("icon.bm", w, h), "no dim suffix fails");
    CHECK(!asset::BitmapAsset::parseDimFromName("bad_axb.bm", w, h), "non-numeric fails");
}

static void test_bitmap_load() {
    std::printf("[BitmapAsset load from VFS]\n");

    MemFileSystem fs;

    // 8x8 checkerboard pattern (alternating pixel rows)
    uint8_t checker[8] = {
        0b10101010,
        0b01010101,
        0b10101010,
        0b01010101,
        0b10101010,
        0b01010101,
        0b10101010,
        0b01010101,
    };
    writeBm(fs, "/icons/test_8x8.bm", checker, 8);

    asset::BitmapAsset img;
    CHECK(img.load(fs, "/icons/test_8x8.bm"), "load by filename dim");
    CHECK(img.valid(), "  valid");
    CHECK(img.width == 8 && img.height == 8, "  dimensions");
    CHECK(img.bits()[0] == 0b10101010, "  first byte matches");

    // Load wrong size
    asset::BitmapAsset img2;
    CHECK(!img2.load(fs, "/icons/test_8x8.bm", 16, 16), "explicit dim mismatch fails");
    CHECK(!img2.valid(), "  invalid");

    // Load missing file
    asset::BitmapAsset img3;
    CHECK(!img3.load(fs, "/nonexistent_8x8.bm"), "missing file fails");
}

static void test_anim_meta() {
    std::printf("[AnimMeta parser]\n");

    const char* txt =
        "Width: 20\n"
        "Height: 20\n"
        "Passive frames: 30\n"
        "Frame rate: 2\n"
        "Active cycles: 2\n"
        "Duration: 60\n";

    auto m = asset::parseAnimMeta(txt);
    CHECK(m.width == 20, "width");
    CHECK(m.height == 20, "height");
    CHECK(m.frameCount == 30, "frameCount");
    CHECK(m.frameRate == 2, "frameRate");
    CHECK(m.loop == true, "loop (active_cycles > 0)");
}

static void test_animation_load() {
    std::printf("[AnimAsset load from VFS]\n");

    MemFileSystem fs;
    ensureDir(fs, "/packs/test");
    ensureDir(fs, "/packs/test/animations");
    ensureDir(fs, "/packs/test/animations/spinner");

    // meta.txt: 8x8, 4 frames, 4 fps
    writeMetaTxt(fs, "/packs/test/animations/spinner", 8, 8, 4, 4);

    // 4 frame .bm files
    uint8_t f0[8] = {0xFF,0x81,0x81,0x81,0x81,0x81,0x81,0xFF};
    uint8_t f1[8] = {0x00,0x7E,0x42,0x42,0x42,0x42,0x7E,0x00};
    uint8_t f2[8] = {0x00,0x00,0x3C,0x24,0x24,0x3C,0x00,0x00};
    uint8_t f3[8] = {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00};

    writeBm(fs, "/packs/test/animations/spinner/frame_0.bm", f0, 8);
    writeBm(fs, "/packs/test/animations/spinner/frame_1.bm", f1, 8);
    writeBm(fs, "/packs/test/animations/spinner/frame_2.bm", f2, 8);
    writeBm(fs, "/packs/test/animations/spinner/frame_3.bm", f3, 8);

    asset::AnimAsset anim;
    CHECK(anim.load(fs, "/packs/test/animations/spinner"), "load animation");
    CHECK(anim.valid(), "  valid");
    CHECK(anim.animation().frameCount == 4, "  frameCount=4");
    CHECK(anim.animation().frameRate == 4, "  frameRate=4");
    CHECK(anim.animation().frames[0].width == 8, "  frame width");
    CHECK(anim.animation().frames[0].height == 8, "  frame height");
    CHECK(anim.animation().frames[2].bitmap[2] == 0x3C, "  frame 2 data matches");
}

static void test_pack_loader() {
    std::printf("[AssetPackLoader]\n");

    MemFileSystem fs;
    ensureDir(fs, "/packs/mypack");
    ensureDir(fs, "/packs/mypack/icons");
    ensureDir(fs, "/packs/mypack/animations");

    // Write icons with _WxH suffix
    uint8_t icon1[8] = {0xFF,0x81,0xA5,0x81,0x81,0x99,0x81,0xFF};
    writeBm(fs, "/packs/mypack/icons/star_8x8.bm", icon1, 8);

    uint8_t icon2[8] = {0x18,0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0x18};
    writeBm(fs, "/packs/mypack/icons/diamond_8x8.bm", icon2, 8);

    asset::AssetPackLoader loader(fs, "/packs/mypack");
    CHECK(loader.isValid(), "pack valid");

    auto icons = loader.listIcons();
    CHECK(icons.size() == 2, "  icon count");
    CHECK(icons[0] == "diamond_8x8.bm" || icons[1] == "diamond_8x8.bm", "  diamond found");

    // Load an icon via pack loader
    auto star = loader.loadIcon("star_8x8.bm");
    CHECK(star.valid(), "  star loaded");
    CHECK(star.width == 8 && star.height == 8, "  star dimensions");

    // Load via full path
    asset::BitmapAsset diamond;
    CHECK(diamond.load(fs, "/packs/mypack/icons/diamond_8x8.bm"), "diamond direct load");
    CHECK(diamond.valid(), "  diamond valid");
}

static void test_icon_registry_fallback() {
    std::printf("[AssetPackRegistry + findIcon fallback]\n");

    MemFileSystem fs;

    uint8_t wifiData[8] = {0x00,0x3C,0x42,0x18,0x24,0x08,0x18,0x00};
    writeBm(fs, "/packs/test/icons/wifi_8x8.bm", wifiData, 8);

    asset::BitmapAsset wifi;
    wifi.load(fs, "/packs/test/icons/wifi_8x8.bm");
    asset::AssetPackRegistry::instance().registerIcon("custom.wifi", wifi);

    // findIcon should still return built-in "status.wifi" first
    const ui::IconDef* builtin = ui::findIcon("status.wifi");
    CHECK(builtin != nullptr, "builtin status.wifi found");

    // Custom icon should be found via registry
    const asset::BitmapAsset* custom = asset::AssetPackRegistry::instance().find("custom.wifi");
    CHECK(custom != nullptr, "custom.wifi in registry");
    CHECK(custom->valid(), "  valid");

    // findIconDef should work
    const ui::IconDef* idef = asset::AssetPackRegistry::instance().findIconDef("custom.wifi");
    CHECK(idef != nullptr, "IconDef bridge for custom.wifi");
    CHECK(idef->w == 8 && idef->h == 8, "  dimensions");
    CHECK(idef->bitmap[1] == wifiData[1], "  data matches");

    // Cleanup
    asset::AssetPackRegistry::instance().clear();
    CHECK(asset::AssetPackRegistry::instance().find("custom.wifi") == nullptr, "clear works");
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    test_dim_parsing();
    test_bitmap_load();
    test_anim_meta();
    test_animation_load();
    test_pack_loader();
    test_icon_registry_fallback();

    std::printf("\n%d failures\n", g_fail);
    return g_fail ? 1 : 0;
}
