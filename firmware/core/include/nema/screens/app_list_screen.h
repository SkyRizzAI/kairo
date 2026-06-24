#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/icon_pack.h"
#include "nema/ui/virtual_list.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;
class AppDetailScreen;

// App list — component-migrated (Plan 30). A VirtualList of installed apps.
//
// Two modes:
//   Launch (default) — activating a row calls rt_.apps().launch(id).
//   Detail — activating a row sets the selected app on the detail screen and
//             pushes it. Enabled by calling setDetailScreen(). Used by
//             Settings → Apps (Plan 87 Fase 7).
//
// Navigation is fully app-managed via VirtualListState::moveFocus() in
// onAction() — not delegated to ComponentRuntime's focus tree.
class AppListScreen : public ComponentScreen {
public:
    explicit AppListScreen(Runtime& rt);
    void onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

    // Switch to Detail mode: activating a row opens `detail` instead of launching.
    void setDetailScreen(AppDetailScreen* detail) { detailScreen_ = detail; }

    // Wire a detail screen for Hold-OK → App Detail from Launch mode.
    void setLaunchDetailScreen(AppDetailScreen* detail) { launchDetail_ = detail; }

    // Handle Prev/Next/Activate/Menu/Back via VirtualListState.
    void onAction(input::Action a) override;

private:
    // Plan 84: bundled 1-bit icon from a .papp app (non-owning pointer).
    struct CustomIcon { const uint8_t* bitmap; uint8_t w, h; };

    // Load and sort (alphabetically, case-insensitive) all installed apps.
    void loadInstalledPapps();

    // Open the detail screen for the currently focused item.
    // In Launch mode: uses launchDetail_ (Hold-OK). In Detail mode: uses detailScreen_.
    void openDetailForFocused();

    // Activate the currently focused item (launch or open detail based on mode).
    void activateFocused();

    // VirtualList renderItem callback (passed as function pointer to VirtualList()).
    static aether::ui::UiNode* renderAppItem(aether::ui::NodeArena& a, int index,
                                              bool focused, void* userdata);

    aether::ui::VirtualListState             vlist_;
    std::vector<std::string>                 names_;
    std::vector<std::string>                 ids_;
    std::vector<const aether::ui::IconDef*>  icons_;       // Plan 53: icon_pack icon
    std::vector<CustomIcon>                  customIcons_;  // Plan 84: bundled raw icon

    AppDetailScreen* detailScreen_ = nullptr;
    AppDetailScreen* launchDetail_ = nullptr;  // detail screen for Hold-OK in Launch mode
};

} // namespace nema
