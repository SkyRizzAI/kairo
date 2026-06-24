// Plan 71 — Dolphin animation showcase app (custom app).
// Plan 82 Phase 5 — migrated from .bm seeding to .panim VFS loading.
#include "nema/apps/dolphin_app.h"
#include "nema/ui/dolphin_anim.h"
#include "nema/app/app_context.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include "nema/input/input_action.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

// ── Loading progress ─────────────────────────────────────────────────────────

void DolphinApp::drawLoading(AppContext& ctx, int done, int total) {
    Canvas& c = ctx.canvas();
    c.clear(false);
    uint16_t W = c.width(), H = c.height();
    FontSpec f = fontForRole(TextRole::Body);
    c.setFont(f.handle);
    uint16_t lh = measureTextH(TextRole::Body);
    c.drawText(4, 4, "Dolphin Showcase", true);
    char line[48];
    std::snprintf(line, sizeof(line), "Loading %d/%d", done, total);
    c.drawText(4, (uint16_t)(H / 2 > lh ? H / 2 - lh : 0), line, true);
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
    IFileSystem* fs = ctx.runtime().fs();
    size_t count = anim::DOLPHIN_ENTRIES_COUNT;

    entries_.clear();
    entries_.reserve(count);

    drawLoading(ctx, 0, (int)count);

    for (size_t ai = 0; ai < count; ai++) {
        drawLoading(ctx, (int)ai, (int)count);
        const anim::DolphinEntry& def = anim::DOLPHIN_ENTRIES[ai];

        Entry e;
        e.name = def.name;
        e.w = def.w; e.h = def.h; e.fps = def.fps;

        if (fs) {
            e.anim = std::make_unique<asset::PanimAsset>();
            if (!e.anim->load(*fs, def.path))
                e.anim.reset();
        }

        if (e.anim) {
            entries_.push_back(std::move(e));
            auto& ee = entries_.back();
            ee.player = std::make_unique<anim::AnimationPlayer>(ee.anim->animation());
            ee.player->start();
        }
    }
}

bool DolphinApp::onTick(AppContext& ctx) {
    (void)ctx;
    if (paused_ || entries_.empty()) return dirty_;

    auto& e = entries_[animIdx_ % entries_.size()];
    if (!e.player) return false;

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
        dirty_ = true;
        return true;
    case Key::Right:
    case Key::Down:
        animIdx_ = (animIdx_ + 1) % (int)entries_.size();
        if (auto& p = entries_[animIdx_ % entries_.size()].player) p->start();
        dirty_ = true;
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

    uint16_t W = c.width(), H = c.height();
    c.clear(false);

    if (entries_.empty()) {
        c.drawText(2, 2, "No animations (VFS not mounted)", true);
        dirty_ = false;
        return true;
    }

    auto& e = entries_[animIdx_ % entries_.size()];

    uint16_t bannerH = (uint16_t)(measureTextH(TextRole::Title) + 2 * t.space.sm);
    char title[80];
    std::snprintf(title, sizeof(title), "%s  [.panim]", e.name.c_str());
    banner(c, 0, 0, W, bannerH, title, false);

    if (e.player && e.anim && e.anim->valid()) {
        const uint8_t* bits = e.player->currentFrameData();
        uint16_t dw = e.anim->w, dh = e.anim->h;

        uint16_t availW = (uint16_t)(W > t.space.sm * 2 ? W - t.space.sm * 2 : W);
        uint16_t availH = (uint16_t)(H > bannerH + t.space.lg * 2 ? H - bannerH - t.space.lg * 2 : H);
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
                if (bits && ((bits[bitIdx / 8] >> (7 - (bitIdx % 8))) & 1))
                    c.drawPixel((uint16_t)(sx + x), (uint16_t)(sy + y), true);
            }
        }
    }

    FontSpec cap = fontForRole(TextRole::Caption);
    c.setFont(cap.handle);
    uint16_t lineH = measureTextH(TextRole::Caption);

    char info[100];
    std::snprintf(info, sizeof(info),
                  "%u/%u  %ux%u  %ufps  %u frames  %s",
                  animIdx_ + 1, (unsigned)entries_.size(),
                  e.w, e.h, e.fps,
                  e.anim ? e.anim->def.frameCount : 0u,
                  paused_ ? "PAUSED" : "PLAY");
    c.drawText(2, (uint16_t)(H - lineH * 2 - 2), info, true);
    char hintBuf[64];
    auto& inp = ctx.runtime().input();
    std::snprintf(hintBuf, sizeof(hintBuf), "%s/%s:switch  %s:%s  %s:exit  [.panim]",
                  inp.hintFor(input::Action::Prev),
                  inp.hintFor(input::Action::Next),
                  inp.hintFor(input::Action::Activate),
                  paused_ ? "play" : "pause",
                  inp.hintFor(input::Action::Back));
    c.drawText(2, (uint16_t)(H - lineH - 1), hintBuf, true);

    dirty_ = false;
    return true;
}

} // namespace nema
