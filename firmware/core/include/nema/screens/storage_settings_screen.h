#pragma once
#include "nema/ui/component_screen.h"
#include "nema/services/storage_service.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// Settings → Storage (Plan 83 Fase 5).
// Shows Internal/SD volume usage and per-app storage info.
// Selecting a movable app toggles its storage location (internal ↔ SD).
class StorageSettingsScreen : public ComponentScreen {
public:
    explicit StorageSettingsScreen(Runtime& rt);
    void onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    struct AppItem {
        StorageSettingsScreen*     self;
        size_t                     idx;   // index into apps_
    };

    static void onSelect(void* u);
    void        moveApp(size_t idx);
    static std::string fmtBytes(size_t bytes);

    aether::ui::ScrollState           scroll_;
    std::vector<StorageService::AppStorageInfo> apps_;
    std::vector<AppItem>              items_;
};

} // namespace nema
