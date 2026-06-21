// Plan 60 — AppListScreen: ListView launcher (banner title + ListItem rows).
// Plan 52/53 — SmartLabel for long names; per-app icon from icon_pack.
#include "nema/screens/app_list_screen.h"
#include "nema/runtime.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/icon_pack.h"
#include "nema/app/app_registry.h"
#include "nema/app/app_manifest.h"
#include "nema/app/papp_installer.h"
#include <cstring>

namespace nema {

using namespace aether::ui;

AppListScreen::AppListScreen(Runtime& rt) : ComponentScreen(rt, 256) {}

// Map category/iconPath to an icon_pack handle.
static const IconDef* iconForApp(const AppManifest& m) {
    // iconPath may directly name an icon handle (e.g. "feature.gpio")
    if (m.iconPath) {
        const IconDef* d = findIcon(m.iconPath);
        if (d) return d;
    }
    // Fallback: map category to a handle
    if (m.category) {
        if (strcmp(m.category, "SubGHz") == 0) return findIcon("feature.subghz");
        if (strcmp(m.category, "NFC")    == 0) return findIcon("feature.nfc");
        if (strcmp(m.category, "GPIO")   == 0) return findIcon("feature.gpio");
        if (strcmp(m.category, "Settings") == 0) return findIcon("feature.settings");
    }
    return findIcon("feature.apps");
}

void AppListScreen::onResume() {
    loadInstalledPapps(rt_);  // auto-scan /flash/apps on every open
    scroll_.scrollMain = 0;
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
        names_.push_back(m.name);
        ids_.push_back(m.id);
        // Prefer bundled raw icon (Plan 84); fall back to icon_pack handle.
        if (m.iconBitmap && m.iconW && m.iconH) {
            customIcons_.push_back({m.iconBitmap, m.iconW, m.iconH});
            icons_.push_back(nullptr);  // unused when customIcons_ entry is valid
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

    uint8_t pad = aether::theme().space.sm;
    uint8_t gap = aether::theme().space.xs;

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = pad; root.gap = gap;
    root.align = Align::Stretch;
    Style sv; sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 1;

    // Row style: horizontal, vertically centered, themed padding + gap
    Style rs; rs.dir = FlexDir::Row; rs.padding = pad; rs.align = Align::Center; rs.gap = gap;
    rs.justify = Justify::SpaceBetween;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (size_t i = 0; i < names_.size(); i++) {
        rows_[i] = {this, (int)i};

        UiNode* row;
        if (empty) {
            row = ListRow(a, names_[i].c_str(), onLaunch, &rows_[i]);
        } else {
            // Icon + SmartLabel (grows) + ">" accessory.
            // Custom bundled icon (Plan 84) takes priority; fallback to icon_pack.
            UiNode* ico_node;
            const auto& ci = customIcons_[i];
            if (ci.bitmap) {
                ico_node = Icon(a, ci.bitmap, ci.w, ci.h);
            } else {
                const IconDef* ico = icons_[i];
                ico_node = ico ? Icon(a, ico->bitmap, ico->w, ico->h) : nullptr;
            }
            UiNode* lbl = SmartLabel(a, names_[i].c_str());
            if (lbl) lbl->style.flexGrow = 1;
            UiNode* acc = Text(a, ">", TextRole::Caption);

            row = a.alloc();
            if (!row) break;
            row->type      = NodeType::Pressable;
            row->style     = rs;
            row->onPress   = onLaunch;
            row->userdata  = &rows_[i];
            row->focusable = true;
            // Link children: [icon?] → lbl → acc
            UiNode* cprev = nullptr;
            auto link = [&](UiNode* child) {
                if (!child) return;
                if (!cprev) row->firstChild = child;
                else        cprev->nextSibling = child;
                cprev = child;
            };
            link(ico_node);
            link(lbl);
            link(acc);
        }
        if (!row) break;
        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
    }

    return View(a, root, {
        TitleBar(a, "APPS"),
        list,
    });
}

} // namespace nema
