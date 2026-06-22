#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/icon_pack.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;
class AppDetailScreen;

// App list — component-migrated (Plan 30). Header + a ScrollView of app rows.
//
// Two modes:
//   Launch (default) — pressing a row calls rt_.apps().launch(id).
//   Detail — pressing a row sets the selected app on the detail screen and
//             pushes it. Enabled by calling setDetailScreen(). Used by
//             Settings → Apps (Plan 87 Fase 7).
class AppListScreen : public ComponentScreen {
public:
    explicit AppListScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

    // Switch to Detail mode: pressing a row opens `detail` instead of launching.
    void setDetailScreen(AppDetailScreen* detail) { detailScreen_ = detail; }

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
    static void onOpenDetail(void* u);

    AppDetailScreen* detailScreen_ = nullptr;
};

} // namespace nema
