#pragma once
// Plan 81 — Live wallpaper desktop skin ("livewal").
// Plan 82 Phase 5 — migrated from C array to PanimAsset filesystem loading.
#include "aether/shell/desktop_theme.h"
#include "nema/ui/animation_player.h"
#include "nema/ui/asset_loader.h"
#include <memory>

namespace nema { class Runtime; }

namespace nema::shell {

class LiveWallpaperDesktop : public IDesktopTheme {
public:
    explicit LiveWallpaperDesktop(nema::Runtime& rt);

    const char* name() const override { return "livewal"; }
    void onResume() override;
    void draw(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    nema::anim::AnimationPlayer* player() override { return player_.get(); }

private:
    nema::Runtime& rt_;
    FitMode fit_    = FitMode::Fit;
    Anchor  anchor_ = Anchor::Center;
    std::unique_ptr<nema::asset::PanimAsset>       asset_;
    std::unique_ptr<nema::anim::AnimationPlayer>   player_;
};

} // namespace nema::shell
