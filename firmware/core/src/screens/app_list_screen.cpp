#include "nema/screens/app_list_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/app/app_registry.h"

namespace nema {

using namespace ui;

AppListScreen::AppListScreen(Runtime& rt) : ComponentScreen(rt, 160) {}

void AppListScreen::enter() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    ComponentScreen::enter();
}

void AppListScreen::onLaunch(void* u) {
    auto* r = static_cast<Row*>(u);
    auto& ids = r->self->ids_;
    if (r->index >= 0 && r->index < (int)ids.size() && !ids[r->index].empty())
        r->self->rt_.apps().launch(ids[r->index].c_str());
}

UiNode* AppListScreen::build(NodeArena& a, Runtime& rt) {
    names_.clear(); ids_.clear(); rows_.clear();
    // Launchable apps only — services (AppType::Service) are background daemons
    // and stay hidden from the launcher (same as Flipper's menu).
    for (const auto& m : rt.apps().list()) {
        if (m.type != AppType::App) continue;
        names_.push_back(m.name);
        ids_.push_back(m.id);
    }
    if (names_.empty()) { names_.push_back("No apps"); ids_.push_back(""); }
    rows_.resize(names_.size());

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    Style line; line.height = 1; line.background = true;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 1;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (size_t i = 0; i < names_.size(); i++) {
        rows_[i] = {this, (int)i};
        UiNode* row = ListRow(a, names_[i].c_str(), onLaunch, &rows_[i]);
        if (!row) break;
        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
    }

    return View(a, root, {
        Text(a, "APPS", TextRole::Title),
        View(a, line, {}),
        list,
    });
}

} // namespace nema
