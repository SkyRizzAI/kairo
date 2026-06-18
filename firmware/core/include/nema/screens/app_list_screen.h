#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/icon_pack.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// App list — component-migrated (Plan 30). Header + a ScrollView of app rows;
// each row is a Pressable that launches the app (tap or focus+Activate).
class AppListScreen : public ComponentScreen {
public:
    explicit AppListScreen(Runtime& rt);
    void        onResume() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    struct Row { AppListScreen* self; int index; };

    ui::ScrollState               scroll_;
    std::vector<std::string>      names_;
    std::vector<std::string>      ids_;
    std::vector<const ui::IconDef*> icons_; // Plan 53: per-app icon (may be nullptr)
    std::vector<Row>              rows_;

    static void onLaunch(void* u);
};

} // namespace nema
