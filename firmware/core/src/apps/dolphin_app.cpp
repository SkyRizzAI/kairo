// Plan 71 — Dolphin animation showcase app with DYNAMIC VFS loading.
// Seeds VFS with .bm files at launch, loads via AnimAsset::load().
// Proves custom apps can load frame animations from filesystem at runtime.
// No AnimationManager — ticks players manually on app thread.

#include "nema/apps/dolphin_app.h"
#include "nema/app/app_context.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/dolphin_anim.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

// ── VFS seeding ──────────────────────────────────────────────────────────────

void DolphinApp::seedDolphinAssets(AppContext& ctx) {
    auto* fs = ctx.runtime().fs();
    if (!fs) return;

    // Write each animation from compiled-in blob to VFS
    size_t count = anim::DOLPHIN_SHOWCASE_COUNT;

    // Seeding writes ~250KB across ~142 files to flash — slow on LittleFS, and
    // the files persist across reboots. Skip if the pack is already present
    // (probe the first animation's meta.txt) so relaunch isn't a 10s black screen.
    if (count > 0) {
        char probe[160];
        std::snprintf(probe, sizeof(probe),
                      "/packs/dolphin/%s/meta.txt", anim::DOLPHIN_META[0].name);
        std::vector<uint8_t> tmp;
        if (fs->read(std::string(probe), tmp)) return;
    }

    for (size_t ai = 0; ai < count; ai++) {
        drawLoading(ctx, (int)ai, (int)count);   // progress while seeding to flash
        auto& meta = anim::DOLPHIN_META[ai];
        auto* anim = anim::DOLPHIN_SHOWCASE[ai];
        if (!anim || meta.frames == 0) continue;

        // Build directory path
        char dir[128];
        std::snprintf(dir, sizeof(dir),
                      "/packs/dolphin/%s", meta.name);
        fs->mkdir("/packs");
        fs->mkdir("/packs/dolphin");
        fs->mkdir(std::string(dir));

        // Write each frame as .bm
        size_t fb = (size_t)((uint32_t)meta.w * meta.h + 7) / 8;
        for (uint8_t fi = 0; fi < meta.frames; fi++) {
            char fname[160];
            std::snprintf(fname, sizeof(fname),
                          "%s/frame_%u.bm", dir, (unsigned)fi);
            fs->write(std::string(fname),
                      anim->frames[fi].bitmap, fb);
        }

        // Write meta.txt
        char metaTxt[256];
        std::snprintf(metaTxt, sizeof(metaTxt),
                      "Width: %u\nHeight: %u\nPassive frames: %u\nFrame rate: %u\n",
                      (unsigned)meta.w, (unsigned)meta.h,
                      (unsigned)meta.frames, (unsigned)meta.fps);
        char metaPath[160];
        std::snprintf(metaPath, sizeof(metaPath), "%s/meta.txt", dir);
        fs->write(std::string(metaPath),
                  (const uint8_t*)metaTxt, std::strlen(metaTxt));
    }
}

// ── Loading screen ───────────────────────────────────────────────────────────

void DolphinApp::drawLoading(AppContext& ctx, int done, int total) {
    Canvas& c = ctx.canvas();
    c.clear(false);
    uint16_t W = c.width();
    uint16_t H = c.height();

    FontSpec f = fontForRole(TextRole::Body);
    c.setFont(f.handle);
    uint16_t lh = measureTextH(TextRole::Body);

    c.drawText(4, 4, "Dolphin Showcase", true);
    char line[48];
    std::snprintf(line, sizeof(line), "Loading %d/%d", done, total);
    c.drawText(4, (uint16_t)(H / 2 > lh ? H / 2 - lh : 0), line, true);

    // Progress bar
    uint16_t bx = 4, bw = (uint16_t)(W > 8 ? W - 8 : W), bh = 6;
    uint16_t by = (uint16_t)(H / 2 + 2);
    c.drawRect(bx, by, bw, bh, true);
    if (total > 0 && done > 0 && bw > 2) {
        uint16_t fw = (uint16_t)((uint32_t)(bw - 2) * (uint32_t)done / (uint32_t)total);
        if (fw) c.fillRect((uint16_t)(bx + 1), (uint16_t)(by + 1), fw, (uint16_t)(bh - 2), true);
    }
    ctx.present();
}

// ── App lifecycle ────────────────────────────────────────────────────────────

void DolphinApp::onStart(AppContext& ctx) {
    drawLoading(ctx, 0, (int)anim::DOLPHIN_SHOWCASE_COUNT);   // immediate feedback
    seedDolphinAssets(ctx);

    auto* fs = ctx.runtime().fs();
    if (!fs) return;

    size_t count = anim::DOLPHIN_SHOWCASE_COUNT;
    // Fresh state each launch. DolphinApp is a persistent singleton, so without
    // this the vector keeps growing on every relaunch — and once it grows past
    // the reserved capacity it REALLOCATES, leaving every AnimationPlayer's
    // `const Animation&` dangling → LoadProhibited crash in currentFrameData().
    entries_.clear();
    entries_.reserve(count);

    for (size_t ai = 0; ai < count; ai++) {
        drawLoading(ctx, (int)ai, (int)count);   // progress while loading from flash
        auto& meta = anim::DOLPHIN_META[ai];

        char dir[128];
        std::snprintf(dir, sizeof(dir),
                      "/packs/dolphin/%s", meta.name);

        Entry e;
        e.name = meta.name;
        if (e.anim.load(*fs, dir)) {
            entries_.push_back(std::move(e));
            // Create player AFTER AnimAsset is in final position
            auto& ee = entries_.back();
            ee.player = std::make_unique<anim::AnimationPlayer>(
                ee.anim.animation());
            ee.player->start();
        }
    }
}

bool DolphinApp::onTick(AppContext& ctx) {
    (void)ctx;
    if (paused_ || entries_.empty()) return dirty_;

    auto& e = entries_[animIdx_ % entries_.size()];
    if (!e.player) return false;

    // Manually tick the player on the app thread (no AnimationManager)
    uint32_t now = (uint32_t)ctx.runtime().clock().millis();
    if (e.player->tick(now)) {
        dirty_ = true;
        return true;
    }
    return false;
}

bool DolphinApp::onKey(Key k, AppContext& ctx) {
    (void)ctx;
    if (entries_.empty()) return false;

    switch (k) {
    case Key::Left:
    case Key::Up:
        animIdx_ = (animIdx_ - 1 + (int)entries_.size()) % (int)entries_.size();
        if (auto& p = entries_[animIdx_ % entries_.size()].player) p->start();
        tickCnt_ = 0;
        dirty_   = true;
        return true;
    case Key::Right:
    case Key::Down:
        animIdx_ = (animIdx_ + 1) % (int)entries_.size();
        if (auto& p = entries_[animIdx_ % entries_.size()].player) p->start();
        tickCnt_ = 0;
        dirty_   = true;
        return true;
    case Key::Select:
        paused_ = !paused_;
        if (auto& p = entries_[animIdx_ % entries_.size()].player) {
            if (paused_) p->pause();
            else         p->start();
        }
        dirty_ = true;
        return true;
    case Key::Cancel:
        return false;
    default:
        return false;
    }
}

bool DolphinApp::drawRaw(Canvas& c, AppContext& ctx) {
    (void)ctx;
    using namespace aether::ui::draw;
    const aether::StyleTokens& t = aether::theme();

    uint16_t W = c.width();
    uint16_t H = c.height();
    c.clear(false);

    if (entries_.empty()) {
        c.drawText(2, 2, "No animations", true);
        dirty_ = false;
        return true;
    }

    auto& e = entries_[animIdx_ % entries_.size()];
    auto& meta = anim::DOLPHIN_META[animIdx_ % anim::DOLPHIN_SHOWCASE_COUNT];

    // Banner
    uint16_t bannerH = (uint16_t)(measureTextH(TextRole::Title) + 2 * t.space.sm);
    char title[80];
    std::snprintf(title, sizeof(title), "%s  [VFS loaded]", e.name.c_str());
    banner(c, 0, 0, W, bannerH, title, false);

    // Render current frame
    if (e.player && e.anim.valid()) {
        const uint8_t* bits = e.player->currentFrameData();
        uint16_t dw = e.player->width();
        uint16_t dh = e.player->height();

        uint16_t availW = (uint16_t)(W > t.space.sm * 2 ? W - t.space.sm * 2 : W);
        uint16_t availH = (uint16_t)(H > bannerH + t.space.lg * 2 ? H - bannerH - t.space.lg * 2 : H);
        // Fit to the available area, preserving aspect ratio — fills the limiting
        // axis so the animation reaches the screen edge. Fixed-point scale (×256)
        // + nearest-neighbor sampling adapts to ANY board resolution (fractional
        // ratios too), not just integer multiples.
        uint32_t sW = dw ? (uint32_t)availW * 256u / dw : 256u;
        uint32_t sH = dh ? (uint32_t)availH * 256u / dh : 256u;
        uint32_t s  = sW < sH ? sW : sH;
        uint16_t sw = (uint16_t)((uint32_t)dw * s / 256u);
        uint16_t sh = (uint16_t)((uint32_t)dh * s / 256u);
        if (!sw) sw = dw;
        if (!sh) sh = dh;
        uint16_t sx = (uint16_t)(W > sw ? (W - sw) / 2 : 0);
        uint16_t sy = (uint16_t)(bannerH + (availH > sh ? (availH - sh) / 2 : 0));

        for (uint16_t y = 0; y < sh; y++) {
            uint16_t srcRow = (uint16_t)((uint32_t)y * dh / sh);
            for (uint16_t x = 0; x < sw; x++) {
                uint16_t srcCol = (uint16_t)((uint32_t)x * dw / sw);
                uint32_t bitIdx = (uint32_t)srcRow * dw + srcCol;
                if ((bits[bitIdx / 8] >> (7 - (bitIdx % 8))) & 1)
                    c.drawPixel((uint16_t)(sx + x), (uint16_t)(sy + y), true);
            }
        }
    }

    // Info
    FontSpec cap = fontForRole(TextRole::Caption);
    c.setFont(cap.handle);
    uint16_t lineH = measureTextH(TextRole::Caption);

    char info[100];
    std::snprintf(info, sizeof(info),
                  "%u/%u  %ux%u  %ufps  %u frames  %s",
                  animIdx_ + 1, (unsigned)entries_.size(),
                  meta.w, meta.h, meta.fps, meta.frames,
                  paused_ ? "PAUSED" : "PLAY");
    c.drawText(2, (uint16_t)(H - lineH * 2 - 2), info, true);
    c.drawText(2, (uint16_t)(H - lineH - 1),
               "< > : switch   OK : pause   Back : exit   [VFS]", true);

    dirty_ = false;
    return true;
}

} // namespace nema
