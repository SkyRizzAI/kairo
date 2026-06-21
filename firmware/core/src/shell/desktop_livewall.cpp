// Plan 81 — Live wallpaper desktop skin implementation.
// Plan 82 Phase 5 — loads first dolphin animation from PanimAsset (.panim FS file).
#include "aether/shell/desktop_livewall.h"
#include "aether/shell/blit.h"
#include "nema/ui/canvas.h"
#include "nema/runtime.h"
#include "nema/config/config_store.h"

namespace nema::shell {

static const char kDefaultAnimPath[] = "anims/boxing.panim";

LiveWallpaperDesktop::LiveWallpaperDesktop(nema::Runtime& rt) : rt_(rt) {}

void LiveWallpaperDesktop::onResume() {
    auto& cfg = rt_.config();
    fit_    = fitFromName(cfg.getString("desktop", "fit", fitName(FitMode::Fit)).c_str(),
                          FitMode::Fit);
    anchor_ = anchorFromName(cfg.getString("desktop", "anchor", anchorName(Anchor::Center)).c_str(),
                             Anchor::Center);

    if (!asset_ && rt_.fs()) {
        asset_ = std::make_unique<nema::asset::PanimAsset>();
        if (!asset_->load(*rt_.fs(), kDefaultAnimPath))
            asset_.reset();
    }

    if (asset_ && !player_) {
        player_ = std::make_unique<nema::anim::AnimationPlayer>(asset_->animation());
        player_->start();
    } else if (player_ && !player_->isPlaying()) {
        player_->start();
    }
}

void LiveWallpaperDesktop::draw(nema::Canvas& c, uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h) {
    c.fillRect(x, y, w, h, false);   // clear region (letterbox / off pixels)
    if (!player_ || !player_->currentFrameData()) return;
    blitFit(c, player_->currentFrameData(), player_->width(), player_->height(),
            x, y, w, h, fit_, anchor_);
}

} // namespace nema::shell
