#pragma once
#include "nema/ui/component_screen.h"
#include "nema/services/storage_service.h"
#include <string>
#include <vector>
#include <cstdint>

namespace nema {

class Runtime;

// Settings → Apps → [App] — per-app detail (Plan 87 Fase 7).
//
// Shows three sections for the selected app:
//   Permissions — one Toggle per sensitive capability; toggling revokes the grant.
//   Storage     — bytes used + move Internal↔SD (when movable and SD present).
//   Uninstall   — removes the .papp bundle and its data.
//
// Call setApp() before pushing this screen.
class AppDetailScreen : public ComponentScreen {
public:
    explicit AppDetailScreen(Runtime& rt);
    void onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

    void setApp(std::string id, std::string displayName,
                const uint8_t* iconBitmap = nullptr,
                uint8_t iconW = 0, uint8_t iconH = 0);

private:
    // Sensitive capabilities to probe + display (all known @tier(sensitive) caps).
    struct CapRow {
        AppDetailScreen* self;
        const char*      cap;
    };

    static void onCapToggle(void* u);
    static void onMove(void* u);
    static void onUninstall(void* u);
    static std::string fmtBytes(size_t bytes);

    std::string  appId_;
    std::string  displayName_;
    const uint8_t* iconBitmap_ = nullptr;
    uint8_t      iconW_ = 0, iconH_ = 0;

    aether::ui::ScrollState            scroll_;
    StorageService::AppStorageInfo     storageInfo_;
    bool                               hasStorageInfo_ = false;
    bool                               loadingStorage_ = false;
    std::vector<std::string>           vals_;   // owns formatted value strings (const char* safety)

    // CapRow entries for Toggle callbacks (indices match kSensitiveCaps order).
    CapRow       capRows_[8];
};

} // namespace nema
