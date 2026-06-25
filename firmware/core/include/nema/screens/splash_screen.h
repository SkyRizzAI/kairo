#pragma once
#include "nema/ui/component_screen.h"

namespace nema {

class Runtime;

// SplashScreen (Plan 92) — the cat logo + a 2-second progress bar. Shown on Aether
// start (Boot → reveals the desktop when done) and before a restart/shutdown
// (Restart/Shutdown → performs it when done). Fullscreen, ignores input.
class SplashScreen : public ComponentScreen {
public:
    enum class Mode { Boot, Restart, Shutdown };
    explicit SplashScreen(Runtime& rt, Mode mode = Mode::Boot);
    void setMode(Mode m) { mode_ = m; }

    void onResume() override;
    void draw(Canvas& c) override;
    void tick(uint64_t nowMs) override;
    void onAction(input::Action) override {}   // swallow input during the splash
    aether::ui::UiNode* build(aether::ui::NodeArena&, Runtime&) override { return nullptr; }

protected:
    bool fullscreen() const override { return true; }

private:
    Mode     mode_;
    uint64_t startMs_  = 0;
    float    progress_ = 0.0f;
    bool     done_     = false;
};

} // namespace nema
