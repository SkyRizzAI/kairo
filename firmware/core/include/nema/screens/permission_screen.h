#pragma once
#include "nema/ui/component_screen.h"
#include "nema/services/permission_service.h"
#include <memory>

namespace nema {

class Runtime;

// PermissionScreen — modal dialog shown when an app calls perm.request() for a
// not-yet-decided capability (Plan 87 Fase 1).
//
// Owned by GuiService (same pattern as LockScreen). GuiService registers a
// ScreenFactory in PermissionService at boot that calls prepare() then
// vd.navigate(*this).
//
// Button callbacks:
//   Allow → req->resolve(1) → goBack()
//   Deny  → req->resolve(2) → goBack()
//
// onStop() is the safety net: if the screen is dismissed without a button click
// (e.g. hardware back on a board that doesn't suppress it for modals), the
// blocked app thread is unblocked with a Deny result so it never hangs.
class PermissionScreen : public ComponentScreen {
public:
    explicit PermissionScreen(Runtime& rt);

    ScreenMode mode()        const override { return ScreenMode::Modal; }
    uint16_t   modalWidth()  const override { return 220; }
    uint16_t   modalHeight() const override { return 100; }

    // Set the active request before pushing. Called by the ScreenFactory
    // (GuiService) on the GUI thread.
    void prepare(std::shared_ptr<PermissionService::PermRequest> req);

    // IScreen
    void onStop() override;

    // ComponentScreen
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    std::shared_ptr<PermissionService::PermRequest> req_;
    char body_[80] = "";

    static void onAllow(void* ctx);
    static void onDeny (void* ctx);
};

} // namespace nema
