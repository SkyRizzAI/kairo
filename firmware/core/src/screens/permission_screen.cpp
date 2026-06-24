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

    snprintf(shortName_, sizeof(shortName_), "\"%s\"", shortNameSrc);
    snprintf(body_,      sizeof(body_),      "\"%s\"", req_->cap.c_str());
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
    uint8_t gap = aether::theme().space.sm;
    uint8_t pad = aether::theme().space.md;

    // Centered column — each Text takes its content width, parent Align::Center
    // positions it horizontally centered (no text-align needed).
    Style col; col.dir = FlexDir::Col; col.align = Align::Center;
    col.padding = pad; col.gap = gap;
    col.height = SIZE_AUTO; col.minH = 50; col.maxH = 100;

    // Button row
    Style bs; bs.dir = FlexDir::Row; bs.border = true; bs.padding = gap;
    bs.align = Align::Center; bs.justify = Justify::Center;
    UiNode* deny  = Pressable(a, onDeny,  this, bs, {Text(a, "Deny",  TextRole::Body)});
    UiNode* allow = Pressable(a, onAllow, this, bs, {Text(a, "Allow", TextRole::Body)});

    Style row; row.dir = FlexDir::Row; row.gap = gap;
    row.align = Align::Center; row.justify = Justify::Center;
    UiNode* btnRow = View(a, row, {});
    if (deny && allow) { btnRow->firstChild = deny; deny->nextSibling = allow; }

    return View(a, col, {
        Text(a, "Permission Request", TextRole::Caption),
        Text(a, shortName_,           TextRole::Title),
        Text(a, "wants to access",    TextRole::Body),
        Text(a, body_,                TextRole::Body),
        btnRow,
    });
}

} // namespace nema
