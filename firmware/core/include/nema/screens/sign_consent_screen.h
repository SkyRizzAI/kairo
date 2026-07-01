#pragma once
#include "nema/ui/component_screen.h"
#include "nema/wallet/wallet_consent_service.h"
#include <memory>

namespace nema {

class Runtime;

// SignConsentScreen — the trusted-display modal for wallet signing (Plan 94, Fase 5/7).
// Shown when a custom app (via nema.wallet.*) requests a signature: it renders the
// SYSTEM's decode of the exact bytes to be signed (WYSIWYS) + the requesting origin, and
// resolves only on a physical Approve/Reject. Owned by GuiService (same pattern as
// PermissionScreen); bound via WalletConsentService::setScreenFactory() at boot.
class SignConsentScreen : public ComponentScreen {
public:
    explicit SignConsentScreen(Runtime& rt);

    ScreenMode mode()        const override { return ScreenMode::Modal; }
    uint16_t   modalWidth()  const override { return 224; }
    uint16_t   modalHeight() const override { return 150; }

    void prepare(std::shared_ptr<wallet::WalletConsentService::SignRequest> req);

    void onStop() override;  // fail-closed: dismissed without a choice → reject
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    std::shared_ptr<wallet::WalletConsentService::SignRequest> req_;
    aether::ui::ScrollState scroll_;   // list scroll (all preview rows, WYSIWYS)
    static void onApprove(void* ctx);
    static void onReject(void* ctx);
    static void onFocusRow(void* ctx);  // no-op: makes a preview row focusable so it marquees
};

}  // namespace nema
