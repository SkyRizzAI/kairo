// Plan 60 — HomeScreen: DSi-style carousel launcher.
// Plan 70 — animated spinner in banner corner.
#include "nema/screens/home_screen.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/animation_manager.h"
#include "nema/app/app_host_manager.h"
#include "nema/input/input_action.h"
#include "nema/runtime.h"
#include <cstdio>

namespace nema {

using namespace ui;

HomeScreen::HomeScreen(Runtime& rt)
    : ComponentScreen(rt), appList_(rt), logs_(rt), settings_(rt) {}

void HomeScreen::onResume() {
    ComponentScreen::onResume();
    nItems_ = hasContinue() ? 4 : 3;
    if (cursor_ >= nItems_) cursor_ = nItems_ - 1;
    if (hasContinue()) {
        const char* name = rt_.appHost().pausedName();
        std::snprintf(continueLabel_, sizeof(continueLabel_),
                      "Continue: %s", name ? name : "app");
    }
    // Plan 70: start the banner spinner and register for global tick
    spinner_.start();
    anim::AnimationManager::instance().registerPlayer(spinner_);
}

bool HomeScreen::hasContinue() const {
    return rt_.appHost().hasPaused();
}

const char* HomeScreen::itemLabel(int i) const {
    if (hasContinue()) {
        if (i == 0) return continueLabel_;
        i--;
    }
    switch (i) {
        case 0: return "Apps";
        case 1: return "Logs";
        case 2: return "Settings";
        default: return "?";
    }
}

void HomeScreen::activate(int i) {
    if (hasContinue()) {
        if (i == 0) { rt_.appHost().resumePaused(); return; }
        i--;
    }
    switch (i) {
        case 0: rt_.view().navigate(appList_);  break;
        case 1: rt_.view().navigate(logs_);     break;
        case 2: rt_.view().navigate(settings_); break;
    }
}

void HomeScreen::onAction(input::Action a) {
    using input::Action;
    switch (a) {
    case Action::Prev:
        if (cursor_ > 0) { cursor_--; requestRedraw(); }
        break;
    case Action::Next:
        if (cursor_ < nItems_ - 1) { cursor_++; requestRedraw(); }
        break;
    case Action::Activate:
        activate(cursor_);
        break;
    default:
        break;
    }
}

// ── DSi carousel draw ─────────────────────────────────────────────────────────

void HomeScreen::draw(Canvas& c) {
    using namespace aether::ui::draw;
    const nema::StyleTokens& t = nema::theme();

    uint16_t W = c.width();
    uint16_t H = c.height();

    // ── Banner ────────────────────────────────────────────────────────────────
    uint16_t bannerH = (uint16_t)(measureTextH(TextRole::Title) + 2 * t.space.sm);
    uint16_t bannerY = CONTENT_Y;
    banner(c, 0, bannerY, W, bannerH, "PALANU", /*notch=*/true);

    // Plan 70: spinner animation in banner right corner
    uint16_t sx = (uint16_t)(W - 12);
    uint16_t sy = (uint16_t)(bannerY + (bannerH > 8 ? (bannerH - 8) / 2 : 0));
    c.drawBitmap(sx, sy, 8, 8, spinner_.currentFrameData());

    // ── Carousel tiles ────────────────────────────────────────────────────────
    uint16_t tileW  = (uint16_t)(W / 2);
    uint16_t tileH  = (uint16_t)(H / 4);
    uint16_t tilesY = (uint16_t)(bannerY + bannerH + t.space.md);
    uint16_t tilesH = (uint16_t)(H - tilesY - t.space.xl);
    uint16_t tileCY = (uint16_t)(tilesY + (tilesH > tileH ? (tilesH - tileH) / 2 : 0));

    // Center tile (focused item) — filled, white label
    uint16_t cx = (uint16_t)((W - tileW) / 2);
    box_rounded(c, cx, tileCY, tileW, tileH, true);
    if (nItems_ > 0) {
        const char* lbl = itemLabel(cursor_);
        FontSpec fs  = fontForRole(TextRole::Title);
        uint16_t tw  = measureTextW(lbl, TextRole::Title);
        uint16_t th  = measureTextH(TextRole::Title);
        uint16_t tx  = (tw < tileW - 4) ? (uint16_t)(cx + (tileW - tw) / 2) : (uint16_t)(cx + 2);
        uint16_t ty  = (uint16_t)(tileCY + (tileH > th ? (tileH - th) / 2 : 0));
        c.setFont(fs.handle);
        if (fs.scale <= 1) c.drawText(tx, ty, lbl, false);
        else               c.drawTextScaled(tx, ty, lbl, fs.scale, false);
    }

    // Side tiles — outline only, smaller
    uint16_t sideW  = (uint16_t)(tileW * 2 / 3);
    uint16_t sideH  = (uint16_t)(tileH * 3 / 4);
    uint16_t sideCY = (uint16_t)(tileCY + (tileH > sideH ? (tileH - sideH) / 2 : 0));
    uint8_t  pad    = t.space.xs;

    if (cursor_ > 0) {
        uint16_t lx  = (uint16_t)(cx > sideW + pad ? cx - pad - sideW : 0);
        box_rounded(c, lx, sideCY, sideW, sideH, false);
        uint16_t th  = measureTextH(TextRole::Body);
        uint16_t ty  = (uint16_t)(sideCY + (sideH > th ? (sideH - th) / 2 : 0));
        ellipsis(c, (uint16_t)(lx + pad), ty, (uint16_t)(sideW - 2 * pad),
                 itemLabel(cursor_ - 1), TextRole::Body);
    }
    if (cursor_ < nItems_ - 1) {
        uint16_t rx  = (uint16_t)(cx + tileW + pad);
        uint16_t rxw = (rx + sideW < W) ? sideW : (uint16_t)(W - rx);
        if (rxw > 4) {
            box_rounded(c, rx, sideCY, rxw, sideH, false);
            uint16_t th = measureTextH(TextRole::Body);
            uint16_t ty = (uint16_t)(sideCY + (sideH > th ? (sideH - th) / 2 : 0));
            ellipsis(c, (uint16_t)(rx + pad), ty, (uint16_t)(rxw - 2 * pad),
                     itemLabel(cursor_ + 1), TextRole::Body);
        }
    }

    // ── Position bar ─────────────────────────────────────────────────────────
    uint16_t posbarY = (uint16_t)(H > t.space.lg ? H - t.space.lg : 0);
    posbar(c, 0, posbarY, W, (uint16_t)cursor_, (uint16_t)nItems_);

    // ── Navigation hints ─────────────────────────────────────────────────────
    FontSpec cap = fontForRole(TextRole::Caption);
    c.setFont(cap.handle);
    uint16_t hintH = measureTextH(TextRole::Caption);
    uint16_t hintY = (uint16_t)(posbarY > hintH + 2 ? posbarY - hintH - 2 : 0);
    const char* hint = rt_.input().hintFor(input::Action::Prev);
    if (!hint || !*hint) hint = "< > : nav   OK : open";
    c.drawText(2, hintY, hint, true);
}

ui::UiNode* HomeScreen::build(ui::NodeArena&, Runtime&) {
    return nullptr;  // carousel draws via draw() override; build() unused
}

} // namespace nema
