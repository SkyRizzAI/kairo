#pragma once
#include "nema/wallet/wallet_types.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

// WalletConsentService — system-owned per-transaction consent (Plan 94, Fase 5, ADR 0014).
//
// The trust anchor for WYSIWYS signing: the requesting worker thread BLOCKS here while
// the GUI thread shows a trusted-display modal (supplied via a ScreenFactory, exactly
// like PermissionService) and the user approves/rejects with a PHYSICAL button. Mirrors
// PermissionService's request/cv/resolve/factory pattern, but consent is NEVER persisted.
//
// Fail-closed: if no screen factory is wired (consent can't be shown), or the request is
// dismissed, the result is reject. The actual SignConsentScreen lives in the display
// layer (aether) and is bound through setScreenFactory() at boot.

namespace nema::wallet {

class WalletConsentService {
public:
    // What the trusted-display modal must show. `preview` is the SYSTEM's decode of the
    // exact bytes to be signed (decode == sign); `origin` is who asked (anti-phishing);
    // `backend` drives the 🔒/⚠️ indicator shown alongside the approve prompt.
    struct SignRequest {
        TxPreview   preview;
        std::string origin;
        BackendKind backend = BackendKind::Software;

        std::mutex              mu;
        std::condition_variable cv;
        bool approved = false;
        bool done     = false;

        void resolve(bool a) {
            std::lock_guard<std::mutex> g(mu);
            approved = a;
            done = true;
            cv.notify_all();
        }
    };

    // The factory closure (captures the display context) pushes the consent screen for a
    // pending request; the screen calls request->resolve(...) on a physical button.
    using ScreenFactory = std::function<void(std::shared_ptr<SignRequest>)>;
    void setScreenFactory(ScreenFactory f) { factory_ = std::move(f); }

    // Worker thread: blocks until the user resolves on the trusted display. Returns true
    // only on explicit approval. Fail-closed: false immediately if no factory is wired or
    // another request is already in flight.
    bool requestSign(const TxPreview& preview, const std::string& origin, BackendKind backend);

    // GUI thread, once per frame: push the consent screen when a request is pending.
    void guiTick();

    // Convenience: a WalletService-compatible confirm callback bound to this service.
    std::function<bool(const TxPreview&)> confirmFor(const std::string& origin, BackendKind backend) {
        return [this, origin, backend](const TxPreview& p) { return requestSign(p, origin, backend); };
    }

private:
    ScreenFactory                factory_;
    std::mutex                   mu_;
    std::shared_ptr<SignRequest> pending_;
    bool                         screenPushed_ = false;
};

}  // namespace nema::wallet
