// Plan 71 — Fullscreen dolphin animation showcase.
// Plan 82 Phase 5 — migrated to PanimAsset (.panim files loaded from VFS).
#include "nema/screens/dolphin_demo.h"
#include "nema/ui/dolphin_anim.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/animation_manager.h"
#include "nema/input/input_action.h"
#include "nema/runtime.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

DolphinDemoScreen::DolphinDemoScreen(Runtime& rt) : ComponentScreen(rt) {}

void DolphinDemoScreen::loadCurrent() {
    if (player_) {
        anim::AnimationManager::instance().unregisterPlayer(*player_);
        player_.reset();
    }
    asset_.reset();

    size_t count = anim::DOLPHIN_ENTRIES_COUNT;
    if (count == 0) return;

    IFileSystem* fs = rt_.fs();
    if (!fs) return;

    const anim::DolphinEntry& e = anim::DOLPHIN_ENTRIES[animIdx_ % count];
    asset_ = std::make_unique<nema::asset::PanimAsset>();
    if (!asset_->load(*fs, e.path)) {
        asset_.reset();
        return;
    }

    player_ = std::make_unique<anim::AnimationPlayer>(asset_->animation());
    if (!paused_) player_->start();
    anim::AnimationManager::instance().registerPlayer(*player_);
}

void DolphinDemoScreen::onResume() {
    ComponentScreen::onResume();
    loadCurrent();
    requestRedraw();
}

void DolphinDemoScreen::onPause() {
    if (player_) {
        anim::AnimationManager::instance().unregisterPlayer(*player_);
        player_->stop();
    }
    ComponentScreen::onPause();
}

void DolphinDemoScreen::onAction(input::Action a) {
    using input::Action;
    size_t count = anim::DOLPHIN_ENTRIES_COUNT;
    switch (a) {
    case Action::Prev:
    case Action::AdjustDown:
        if (count > 0) {
            animIdx_ = (animIdx_ - 1 + (int)count) % (int)count;
            loadCurrent();
            requestRedraw();
        }
        break;
    case Action::Next:
    case Action::AdjustUp:
        if (count > 0) {
            animIdx_ = (animIdx_ + 1) % (int)count;
            loadCurrent();
            requestRedraw();
        }
        break;
    case Action::Activate:
        paused_ = !paused_;
        if (player_) {
            if (paused_) player_->pause();
            else player_->start();
        }
        requestRedraw();
        break;
    case Action::Back:
        rt_.view().goBack();
        break;
    default:
        break;
    }
}

void DolphinDemoScreen::draw(Canvas& c) {
    using namespace aether::ui::draw;
    const aether::StyleTokens& t = aether::theme();

    uint16_t W = c.width();
    uint16_t H = c.height();
    c.clear(false);

    size_t count = anim::DOLPHIN_ENTRIES_COUNT;
    if (count == 0 || !asset_) {
        c.drawText(2, 2, "No animations (VFS not mounted)", true);
        return;
    }

    const anim::DolphinEntry& entry = anim::DOLPHIN_ENTRIES[animIdx_ % count];

    // Banner
    uint16_t bannerH = (uint16_t)(measureTextH(TextRole::Title) + 2 * t.space.sm);
    banner(c, 0, 0, W, bannerH, entry.name, false);

    // Animation — scaled to fill
    if (player_) {
        const uint8_t* bits = player_->currentFrameData();
        uint16_t dw = asset_->w;
        uint16_t dh = asset_->h;

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

    // Info bar
    FontSpec cap = fontForRole(TextRole::Caption);
    c.setFont(cap.handle);
    uint16_t lineH = measureTextH(TextRole::Caption);

    char info[80];
    std::snprintf(info, sizeof(info),
                  "%u/%u  %ux%u  %ufps  %u frames  %s",
                  animIdx_ + 1, (unsigned)count,
                  entry.w, entry.h, entry.fps,
                  asset_ ? asset_->def.frameCount : 0u,
                  paused_ ? "PAUSED" : "PLAY");
    c.drawText(2, (uint16_t)(H - lineH * 2 - 2), info, true);
    char hintBuf[64];
    std::snprintf(hintBuf, sizeof(hintBuf), "%s/%s:switch  %s:%s  %s:exit",
                  rt_.input().hintFor(input::Action::Prev),
                  rt_.input().hintFor(input::Action::Next),
                  rt_.input().hintFor(input::Action::Activate),
                  paused_ ? "play" : "pause",
                  rt_.input().hintFor(input::Action::Back));
    c.drawText(2, (uint16_t)(H - lineH - 1), hintBuf, true);
}

} // namespace nema
