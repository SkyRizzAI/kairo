#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/animation_player.h"
#include <memory>

namespace nema {

class Runtime;

// Plan 71 — Fullscreen dolphin animation showcase.
// Cycles through 10 Flipper Zero dolphin animations. Prev/Next to switch,
// Activate to toggle pause, Back to exit.
class DolphinDemoScreen : public ComponentScreen {
public:
    explicit DolphinDemoScreen(Runtime& rt);

    void onResume() override;
    void onPause() override;
    void draw(Canvas& c) override;
    void onAction(input::Action a) override;
    ui::UiNode* build(ui::NodeArena&, Runtime&) override { return nullptr; }

private:
    std::unique_ptr<anim::AnimationPlayer> player_;
    int  animIdx_ = 0;
    bool paused_  = false;
    void loadCurrent();
};

} // namespace nema
