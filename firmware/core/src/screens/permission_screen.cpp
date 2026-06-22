#include "nema/screens/permission_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/view_dispatcher.h"
#include <cstdio>

namespace nema {

PermissionScreen::PermissionScreen(Runtime& rt) : ComponentScreen(rt) {}

void PermissionScreen::prepare(std::shared_ptr<PermissionService::PermRequest> req) {
    req_ = std::move(req);
    snprintf(body_, sizeof(body_), "%s\nrequests access to:\n%s",
             req_->appId.c_str(), req_->cap.c_str());
    dirty_ = true;
}

void PermissionScreen::onStop() {
    // Safety fallback: if the screen is dismissed without a button click (e.g.
    // a hardware Back action on a board that doesn't block it for modals), treat
    // it as Deny so the blocked app thread always unblocks.
    if (req_ && !req_->done) req_->resolve(2);
    req_.reset();
}

void PermissionScreen::onAllow(void* ctx) {
    auto* self = static_cast<PermissionScreen*>(ctx);
    if (self->req_) self->req_->resolve(1);
    self->rt_.view().goBack();
}

void PermissionScreen::onDeny(void* ctx) {
    auto* self = static_cast<PermissionScreen*>(ctx);
    if (self->req_) self->req_->resolve(2);
    self->rt_.view().goBack();
}

aether::ui::UiNode* PermissionScreen::build(aether::ui::NodeArena& a, Runtime&) {
    aether::ui::DialogButton buttons[2] = {
        {"Allow", onAllow, this},
        {"Deny",  onDeny,  this},
    };
    return aether::ui::Dialog(a, "Permission Request", body_,
                              nullptr, 0, 0, buttons, 2);
}

} // namespace nema
