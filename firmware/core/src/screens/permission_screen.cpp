#include "nema/screens/permission_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/view_dispatcher.h"
#include <cstdio>

namespace nema {

PermissionScreen::PermissionScreen(Runtime& rt) : ComponentScreen(rt) {}

void PermissionScreen::prepare(std::shared_ptr<PermissionService::PermRequest> req) {
    req_ = std::move(req);

    // Extract short name: last segment after the final '.' in the bundle ID.
    const std::string& full = req_->appId;
    size_t dot = full.rfind('.');
    const char* shortNameSrc = (dot != std::string::npos && dot + 1 < full.size())
                               ? full.c_str() + dot + 1
                               : full.c_str();

    snprintf(shortName_, sizeof(shortName_), "%s", shortNameSrc);
    snprintf(body_,      sizeof(body_),      "wants to access: %s", req_->cap.c_str());
    dirty_ = true;
}

void PermissionScreen::onStop() {
    // Safety fallback: if the screen is dismissed without a button click (e.g.
    // Back on a board that doesn't block it for modals), resolve with 0 ("skip").
    // Result 0 is NOT persisted by PermissionService, so the dialog appears again
    // on the next request() — Back means "not now", not "never".
    if (req_ && !req_->done) req_->resolve(0);
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
    using namespace aether::ui;
    DialogButton btns[2] = {
        {"Deny",  onDeny,  this, false},
        {"Allow", onAllow, this, false},
    };
    return Dialog(a, shortName_, body_, nullptr, 0, 0, btns, 2);
}

} // namespace nema
