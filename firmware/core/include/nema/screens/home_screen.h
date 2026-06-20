#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/animation_player.h"
#include "nema/ui/builtin_animations.h"
#include "nema/screens/app_list_screen.h"
#include "nema/screens/file_browser_screen.h"
#include "nema/screens/dolphin_demo.h"
#include "nema/screens/logs_screen.h"
#include "nema/screens/settings_screen.h"

namespace nema {

class Runtime;

// HomeScreen — DSi-style carousel launcher (Plan 60).
// Items: [Continue?] [Apps] [Files] [Dolphin] [Logs] [Settings]
class HomeScreen : public ComponentScreen {
public:
    explicit HomeScreen(Runtime& rt);

    void        onResume() override;
    void        draw(Canvas& c) override;
    void        onAction(input::Action a) override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    AppListScreen     appList_;
    FileBrowserScreen files_;
    DolphinDemoScreen dolphin_;
    LogsScreen        logs_;
    SettingsScreen    settings_;

    int cursor_ = 0;
    int nItems_ = 0;

    char continueLabel_[40] = "Continue";

    anim::AnimationPlayer spinner_{ anim::A_SPINNER };

    bool hasContinue() const;
    const char* itemLabel(int i) const;
    void activate(int i);
};

} // namespace nema
