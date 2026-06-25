// Plan 81 — Live wallpaper desktop skin implementation.
// Plan 82 Phase 5 — loads first dolphin animation from PanimAsset (.panim FS file).
#include "aether/shell/desktop_livewall.h"
#include "aether/shell/blit.h"
#include "nema/ui/canvas.h"
#include "nema/runtime.h"
#include "nema/config/config_store.h"

namespace nema::shell {

static const char kDefaultAnim[] = "laptop";   // config "desktop"/"anim" default

// Load <anim>.panim from /system/assets/anims into a fresh asset+player.
// Returns true on success (asset_/player_/currentAnim_ swapped to the new one).
bool LiveWallpaperDesktop::loadAnim(const std::string& anim) {
    if (!rt_.fs()) return false;
    std::string path = "system/assets/anims/" + anim + ".panim";
    auto asset = std::make_unique<nema::asset::PanimAsset>();
    if (!asset->load(*rt_.fs(), path.c_str())) return false;
    asset_       = std::move(asset);
    player_      = std::make_unique<nema::anim::AnimationPlayer>(asset_->animation());
    currentAnim_ = anim;
    player_->start();
    return true;
}

LiveWallpaperDesktop::LiveWallpaperDesktop(nema::Runtime& rt) : rt_(rt) {}

void LiveWallpaperDesktop::onResume() {
    auto& cfg = rt_.config();
    fit_    = fitFromName(cfg.getString("desktop", "fit", fitName(FitMode::Fit)).c_str(),
                          FitMode::Fit);
    anchor_ = anchorFromName(cfg.getString("desktop", "anchor", anchorName(Anchor::Center)).c_str(),
                             Anchor::Center);

    // Selectable live wallpaper (config "desktop"/"anim", saved by Appearances).
    // Reload when the selection changed (or nothing is loaded yet); fall back to
    // the default if the chosen file is missing.
    std::string anim = cfg.getString("desktop", "anim", kDefaultAnim);
    if (anim != currentAnim_ || !asset_) {
        if (!loadAnim(anim) && anim != kDefaultAnim)
            loadAnim(kDefaultAnim);
    }

    if (player_ && !player_->isPlaying())
        player_->start();
}

void LiveWallpaperDesktop::draw(nema::Canvas& c, uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h) {
    c.fillRect(x, y, w, h, false);   // clear region (letterbox / off pixels)
    if (!player_ || !player_->currentFrameData()) return;
    blitFit(c, player_->currentFrameData(), player_->width(), player_->height(),
            x, y, w, h, fit_, anchor_);
}

} // namespace nema::shell
