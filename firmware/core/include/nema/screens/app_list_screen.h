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
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    struct Row { AppListScreen* self; int index; };

    // Plan 84: bundled 1-bit icon from a .papp app (non-owning pointer).
    struct CustomIcon { const uint8_t* bitmap; uint8_t w, h; };

    aether::ui::ScrollState                 scroll_;
    std::vector<std::string>                names_;
    std::vector<std::string>                ids_;
    std::vector<const aether::ui::IconDef*> icons_;        // Plan 53: icon_pack icon
    std::vector<CustomIcon>                 customIcons_;   // Plan 84: bundled raw icon
    std::vector<Row>                        rows_;

    static void onLaunch(void* u);
};

} // namespace nema
