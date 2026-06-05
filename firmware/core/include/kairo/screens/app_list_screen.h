#pragma once
#include "kairo/ui/component_screen.h"
#include <vector>
#include <string>

namespace kairo {

class Runtime;

// App list — component-migrated (Plan 30). Header + a ScrollView of plugin rows;
// each row is a Pressable that launches the plugin (tap or focus+Activate).
class AppListScreen : public ComponentScreen {
public:
    explicit AppListScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    struct Row { AppListScreen* self; int index; };

    ui::ScrollState          scroll_;
    std::vector<std::string> names_;
    std::vector<std::string> ids_;
    std::vector<Row>         rows_;   // stable per-frame callback contexts

    static void onLaunch(void* u);
};

} // namespace kairo
