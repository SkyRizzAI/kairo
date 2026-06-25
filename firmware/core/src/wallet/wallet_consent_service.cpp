#include "nema/wallet/wallet_consent_service.h"

namespace nema::wallet {

bool WalletConsentService::requestSign(const TxPreview& preview, const std::string& origin,
                                       BackendKind backend) {
    std::shared_ptr<SignRequest> req;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!factory_) return false;   // can't show consent → fail-closed
        if (pending_) return false;    // one trusted-display prompt at a time (serialize)
        req = std::make_shared<SignRequest>();
        req->preview = preview;
        req->origin = origin;
        req->backend = backend;
        pending_ = req;
        screenPushed_ = false;
    }

    // Block this worker thread until the GUI resolves (physical button / dismissal).
    {
        std::unique_lock<std::mutex> g(req->mu);
        req->cv.wait(g, [&] { return req->done; });
    }
    bool approved = req->approved;

    {
        std::lock_guard<std::mutex> lk(mu_);
        pending_.reset();
        screenPushed_ = false;
    }
    return approved;
}

void WalletConsentService::guiTick() {
    std::shared_ptr<SignRequest> req;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!pending_ || screenPushed_ || !factory_) return;
        req = pending_;
        screenPushed_ = true;
    }
    factory_(req);  // display layer pushes the SignConsentScreen for this request
}

}  // namespace nema::wallet
