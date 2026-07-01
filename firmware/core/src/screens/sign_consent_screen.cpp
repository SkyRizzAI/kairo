#include "nema/screens/sign_consent_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/view_dispatcher.h"

namespace nema {

SignConsentScreen::SignConsentScreen(Runtime& rt) : ComponentScreen(rt) {}

void SignConsentScreen::prepare(std::shared_ptr<wallet::WalletConsentService::SignRequest> req) {
    req_ = std::move(req);
    dirty_ = true;
}

void SignConsentScreen::onStop() {
    if (req_ && !req_->done) req_->resolve(false);  // fail-closed
    req_.reset();
}

void SignConsentScreen::onApprove(void* ctx) {
    auto* self = static_cast<SignConsentScreen*>(ctx);
    if (self->req_) self->req_->resolve(true);
    self->rt_.view().goBack();
}

void SignConsentScreen::onReject(void* ctx) {
    auto* self = static_cast<SignConsentScreen*>(ctx);
    if (self->req_) self->req_->resolve(false);
    self->rt_.view().goBack();
}

// A preview row's onPress does nothing — it exists only so the row is a focus stop,
// which lets a long value (e.g. a full destination address) marquee-scroll when
// focused, so WYSIWYS never hides part of what's being signed behind an ellipsis.
void SignConsentScreen::onFocusRow(void*) {}

aether::ui::UiNode* SignConsentScreen::build(aether::ui::NodeArena& a, Runtime&) {
    using namespace aether::ui;

    // Same list language as every settings screen: a ListSection subheader + rows in a
    // scrolling ListContainer. All preview rows are shown (no cap) and the whole list
    // scrolls, so a richer decode (BTC To/Amount/Fee, EVM ERC-20) is fully visible.
    MenuBuilder m(a, scroll_, this);
    m.section("Sign request");
    if (req_) {
        if (!req_->origin.empty()) m.info("From", req_->origin.c_str());
        m.info("Key", req_->backend == wallet::BackendKind::SecureElement
                          ? "Secure Element" : "Software key");
        if (req_->preview.blindSign)
            m.info("Warning", "Blind sign - not decoded!");
        // Every decoded row, focusable so long values marquee (strings live in req_).
        for (auto& r : req_->preview.rows) {
            ListEntry e;
            e.label = r.label.c_str();
            e.value = r.value.c_str();
            e.onPress = onFocusRow;   // focusable display row (marquee), no action
            e.user = this;
            m.add(ListItemRow(a, e));
        }
    }
    // Actions — Reject before Approve so the safer choice is the first action reached.
    m.section("Confirm");
    ListEntry rej; rej.label = "Reject";  rej.onPress = onReject;  rej.user = this;
    ListEntry apr; apr.label = "Approve"; apr.onPress = onApprove; apr.user = this;
    m.add(ListItemRow(a, rej));
    m.add(ListItemRow(a, apr));
    return m.build();
}

}  // namespace nema
