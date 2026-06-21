#pragma once
// Plan 81 — Live wallpaper desktop skin ("livewal").
#include "aether/shell/desktop_theme.h"
#include "nema/ui/animation_player.h"
#include "nema/ui/dolphin_anim.h"

namespace nema { class Runtime; }

namespace nema::shell {

class LiveWallpaperDesktop : public IDesktopTheme {
public:
    explicit LiveWallpaperDesktop(nema::Runtime& rt);

    const char* name() const override { return "livewal"; }
    void onResume() override;
    void draw(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    nema::anim::AnimationPlayer* player() override { return &player_; }

private:
    nema::Runtime& rt_;
    FitMode fit_    = FitMode::Fit;
    Anchor  anchor_ = Anchor::Center;
    nema::anim::AnimationPlayer player_{ *nema::anim::DOLPHIN_SHOWCASE[0] };
};

} // namespace nema::shell
