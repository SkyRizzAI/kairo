#include "nema/screens/permission_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/style_tokens.h"
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
    snprintf(cap_,       sizeof(cap_),       "%s", req_->cap.c_str());
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
    // Build separate Text nodes per line — a single string with \n works for
    // layout height but the renderer only draws the first line, leaving a blank
    // gap. One Text node per line gives correct rendering.
    using namespace aether::ui;
    uint8_t pad = aether::theme().space.md;
    uint8_t gap = aether::theme().space.sm;

    UiNode* hdr   = Text(a, "Permission Request", TextRole::Caption);
    UiNode* app   = Text(a, shortName_,           TextRole::Body);
    UiNode* lbl   = Text(a, "to access:",         TextRole::Caption);
    UiNode* cap   = Text(a, cap_,                 TextRole::Caption);
    UiNode* allow = Button(a, "Allow", onAllow, this);
    UiNode* deny  = Button(a, "Deny",  onDeny,  this);

    Style rowS; rowS.dir = FlexDir::Row; rowS.gap = gap;
    rowS.align = Align::Center; rowS.justify = Justify::Center;
    UiNode* btnRow = View(a, rowS, {});
    btnRow->firstChild  = allow;
    allow->nextSibling  = deny;

    Style colS; colS.dir = FlexDir::Col; colS.padding = pad; colS.gap = gap;
    colS.align = Align::Center;
    UiNode* root = View(a, colS, {});
    root->firstChild   = hdr;
    hdr->nextSibling   = app;
    app->nextSibling   = lbl;
    lbl->nextSibling   = cap;
    cap->nextSibling   = btnRow;
    return root;
}

} // namespace nema
