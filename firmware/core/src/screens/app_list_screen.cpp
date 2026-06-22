// Plan 60 — AppListScreen: standardised-UI list of installed apps.
// Plan 52/53 — per-app icon from icon_pack or bundled bitmap (Plan 84).
#include "nema/screens/app_list_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/icon_pack.h"
#include "nema/app/app_registry.h"
#include "nema/app/app_manifest.h"
#include "nema/app/papp_installer.h"
#include <cstring>

namespace nema {

using namespace aether::ui;

AppListScreen::AppListScreen(Runtime& rt) : ComponentScreen(rt, 512) {}

static const IconDef* iconForApp(const AppManifest& m) {
    if (m.iconPath) {
        const IconDef* d = findIcon(m.iconPath);
        if (d) return d;
    }
    if (m.category) {
        if (strcmp(m.category, "SubGHz")   == 0) return findIcon("feature.subghz");
        if (strcmp(m.category, "NFC")      == 0) return findIcon("feature.nfc");
        if (strcmp(m.category, "GPIO")     == 0) return findIcon("feature.gpio");
        if (strcmp(m.category, "Settings") == 0) return findIcon("feature.settings");
    }
    return findIcon("feature.apps");
}

void AppListScreen::onResume() {
    loadInstalledPapps(rt_);
    scroll_.scrollMain   = 0;
    state_.focus.focused = 0;
    ComponentScreen::onResume();
}

void AppListScreen::onLaunch(void* u) {
    auto* r = static_cast<Row*>(u);
    auto& ids = r->self->ids_;
    if (r->index >= 0 && r->index < (int)ids.size() && !ids[r->index].empty())
        r->self->rt_.apps().launch(ids[r->index].c_str());
}

UiNode* AppListScreen::build(NodeArena& a, Runtime& rt) {
    names_.clear(); ids_.clear(); icons_.clear(); customIcons_.clear(); rows_.clear();

    for (const auto& m : rt.apps().list()) {
        if (m.type != AppType::App) continue;
        if (m.category && std::strcmp(m.category, "System") == 0) continue;
        names_.push_back(m.name);
        ids_.push_back(m.id);
        if (m.iconBitmap && m.iconW && m.iconH) {
            customIcons_.push_back({m.iconBitmap, m.iconW, m.iconH});
            icons_.push_back(nullptr);
        } else {
            customIcons_.push_back({nullptr, 0, 0});
            icons_.push_back(iconForApp(m));
        }
    }

    bool empty = names_.empty();
    if (empty) {
        names_.push_back("No apps installed");
        ids_.push_back("");
        icons_.push_back(nullptr);
        customIcons_.push_back({nullptr, 0, 0});
    }
    rows_.resize(names_.size());

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;

    for (size_t i = 0; i < names_.size(); i++) {
        rows_[i] = {this, (int)i};

        ListEntry e;
        e.label   = names_[i].c_str();
        e.chevron = !empty;
        e.onPress = empty ? nullptr : onLaunch;
        e.user    = &rows_[i];

        const auto& ci = customIcons_[i];
        if (ci.bitmap) {
            e.leftIcon = ci.bitmap; e.iconW = ci.w; e.iconH = ci.h;
        } else if (icons_[i]) {
            e.leftIcon = icons_[i]->bitmap;
            e.iconW    = icons_[i]->w;
            e.iconH    = icons_[i]->h;
        }

        UiNode* row = ListItemRow(a, e);
        if (!row) break;
        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
    }

    return View(a, root, { list });
}

} // namespace nema
