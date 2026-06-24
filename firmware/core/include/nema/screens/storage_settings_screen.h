#pragma once
#include "nema/ui/component_screen.h"
#include "nema/services/storage_service.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// Settings → Storage (Plan 83 Fase 5, updated Plan 89).
// Shows Internal/SD volume usage. Data is loaded asynchronously in onResume()
// via TaskRunner so the screen is never blocked on a VFS scan.
class StorageSettingsScreen : public ComponentScreen {
public:
    explicit StorageSettingsScreen(Runtime& rt);
    void onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    static std::string fmtBytes(size_t bytes);
    static void onEjectSd(void* u);

    // Cached data loaded asynchronously. `ready` is set by the done callback
    // (UI thread), never by the worker — so build() may safely read all fields
    // once ready == true.
    struct StorageData {
        StorageService::VolumeInfo  intVol;
        StorageService::VolumeInfo  extVol;
        StorageService::SdCardInfo  sdInfo;
        bool ready = false;
    };

    aether::ui::ScrollState  scroll_;
    std::vector<std::string> vals_;
    StorageData              cached_;
    int                      intCapPct_ = 0;   // 0-100, drives read-only Slider
    int                      sdCapPct_  = 0;
};

} // namespace nema
