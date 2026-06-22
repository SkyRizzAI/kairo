#pragma once
#include "nema/ui/component_screen.h"
#include "nema/services/storage_service.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// Settings → Storage (Plan 83 Fase 5, updated Plan 87 Fase 7).
// Shows Internal/SD volume usage only.
// Per-app detail (permissions + move + uninstall) lives in AppDetailScreen,
// reached via Settings → Apps → [app].
class StorageSettingsScreen : public ComponentScreen {
public:
    explicit StorageSettingsScreen(Runtime& rt);
    void onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    static std::string fmtBytes(size_t bytes);

    aether::ui::ScrollState  scroll_;
    std::vector<std::string> vals_;   // owns formatted value strings (const char* safety)
};

} // namespace nema
