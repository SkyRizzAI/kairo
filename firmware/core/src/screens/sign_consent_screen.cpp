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

aether::ui::UiNode* SignConsentScreen::build(aether::ui::NodeArena& a, Runtime&) {
    using namespace aether::ui;
    uint8_t gap = aether::theme().space.sm;
    uint8_t pad = aether::theme().space.md;

    Style col; col.dir = FlexDir::Col; col.align = Align::Stretch;
    col.padding = pad; col.gap = gap;

    // Buttons
    Style bs; bs.dir = FlexDir::Row; bs.border = true; bs.padding = gap;
    bs.align = Align::Center; bs.justify = Justify::Center;
    UiNode* reject  = Pressable(a, onReject,  this, bs, {Text(a, "Reject",  TextRole::Body)});
    UiNode* approve = Pressable(a, onApprove, this, bs, {Text(a, "Approve", TextRole::Body)});
    Style rowS; rowS.dir = FlexDir::Row; rowS.gap = gap;
    rowS.align = Align::Center; rowS.justify = Justify::Center;
    UiNode* btnRow = View(a, rowS, {});
    if (reject && approve) { btnRow->firstChild = reject; reject->nextSibling = approve; }

    UiNode* root = View(a, col, {});
    UiNode* prev = nullptr;
    auto add = [&](UiNode* n) {
        if (!n) return;
        if (!root->firstChild) root->firstChild = n;
        else prev->nextSibling = n;
        prev = n;
    };

    add(Text(a, "Sign request", TextRole::Caption));
    if (req_) {
        if (!req_->origin.empty()) add(Text(a, req_->origin.c_str(), TextRole::Body));
        add(Text(a, req_->backend == wallet::BackendKind::SecureElement ? "Secure Element" : "Software key",
                 TextRole::Caption));
        int shown = 0;
        for (auto& r : req_->preview.rows) {       // strings live in req_ while the screen is up
            if (shown++ >= 3) break;
            Style lr; lr.dir = FlexDir::Row; lr.gap = gap; lr.justify = Justify::SpaceBetween;
            add(View(a, lr, {Text(a, r.label.c_str(), TextRole::Caption),
                             Text(a, r.value.c_str(), TextRole::Body)}));
        }
        if (req_->preview.blindSign) add(Text(a, "Blind sign - not decoded!", TextRole::Body));
    }
    add(btnRow);
    return root;
}

}  // namespace nema
