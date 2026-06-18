#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/animation_player.h"
#include "nema/ui/builtin_animations.h"
#include "nema/screens/app_list_screen.h"
#include "nema/screens/logs_screen.h"
#include "nema/screens/settings_screen.h"

namespace nema {

class Runtime;

// HomeScreen — DSi-style carousel launcher (Plan 60).
// Items: [Continue?] [Apps] [Logs] [Settings]
// Carousel shows current item large + adjacent items smaller at screen edges.
// Draws via tier-1 draw toolkit (not ComponentScreen default renderer).
// Plan 70: animated spinner in the banner corner.
class HomeScreen : public ComponentScreen {
public:
    explicit HomeScreen(Runtime& rt);

    void        onResume() override;
    void        draw(Canvas& c) override;        // override: carousel render
    void        onAction(input::Action a) override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;  // unused stub

private:
    AppListScreen  appList_;
    LogsScreen     logs_;
    SettingsScreen settings_;

    int cursor_  = 0;
    int nItems_  = 0;    // set in enter() based on hasPaused()

    char continueLabel_[40] = "Continue";

    // Plan 70: animated spinner in banner
    anim::AnimationPlayer spinner_{ anim::A_SPINNER };

    // Returns true if the "Continue" entry should be shown.
    bool hasContinue() const;
    // Label for item at index i.
    const char* itemLabel(int i) const;
    // Navigate to sub-screen for item at index i.
    void activate(int i);
};

} // namespace nema
