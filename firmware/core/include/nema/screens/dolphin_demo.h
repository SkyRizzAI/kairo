#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/animation_player.h"
#include "nema/ui/asset_loader.h"
#include <memory>

namespace nema {

class Runtime;

// Plan 71 — Fullscreen dolphin animation showcase.
// Plan 82 Phase 5 — migrated to PanimAsset (.panim files from VFS).
// Cycles through dolphin animations. Prev/Next to switch, Activate to pause.
class DolphinDemoScreen : public ComponentScreen {
public:
    explicit DolphinDemoScreen(Runtime& rt);

    void onResume() override;
    void onPause() override;
    void draw(Canvas& c) override;
    void onAction(input::Action a) override;
    aether::ui::UiNode* build(aether::ui::NodeArena&, Runtime&) override { return nullptr; }

protected:
    bool fullscreen() const override { return true; }

private:
    std::unique_ptr<nema::asset::PanimAsset>        asset_;
    std::unique_ptr<anim::AnimationPlayer>           player_;
    int  animIdx_ = 0;
    bool paused_  = false;
    void loadCurrent();
};

} // namespace nema
