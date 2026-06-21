// Plan 83 Fase 5 — StorageSettingsScreen: volume overview + per-app list + move.
#include "nema/screens/storage_settings_screen.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/ui/view_dispatcher.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

StorageSettingsScreen::StorageSettingsScreen(Runtime& rt)
    : ComponentScreen(rt, 160) {}

void StorageSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    apps_.clear();
    items_.clear();
    vals_.clear();
    rt_.view().requestRedraw();
}

std::string StorageSettingsScreen::fmtBytes(size_t bytes) {
    char buf[24];
    if (bytes >= 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%zu MB", bytes / (1024 * 1024));
    else if (bytes >= 1024)
        std::snprintf(buf, sizeof(buf), "%zu KB", bytes / 1024);
    else
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    return buf;
}

void StorageSettingsScreen::onSelect(void* u) {
    auto* it = static_cast<AppItem*>(u);
    it->self->moveApp(it->idx);
}

void StorageSettingsScreen::moveApp(size_t idx) {
    auto* svc = rt_.container().resolve<StorageService>();
    if (!svc || idx >= apps_.size()) return;
    auto& app = apps_[idx];
    if (!app.movable) return;
    StorageLocation target = (app.location == StorageLocation::External)
                                 ? StorageLocation::Internal
                                 : StorageLocation::External;
    svc->move(app.bundleId.c_str(), target);
    rt_.view().requestRedraw();
}

aether::ui::UiNode* StorageSettingsScreen::build(NodeArena& a, Runtime& rt) {
    apps_.clear();
    items_.clear();

    auto* svc = rt.container().resolve<StorageService>();

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;
    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    if (!svc) {
        ListEntry e; e.label = "Not available";
        append(ListItemRow(a, e));
        return View(a, root, { list });
    }

    // vals_ owns all formatted value strings — ListEntry holds const char* into them.
    vals_.clear();

    auto pushVal = [&](const std::string& s) -> const char* {
        vals_.push_back(s);
        return vals_.back().c_str();
    };

    // ── Volume overview ──────────────────────────────────────────────────────
    append(ListSection(a, "Storage"));

    auto intVol = svc->internalVolume();
    {
        std::string v = intVol.totalBytes > 0
            ? fmtBytes(intVol.usedBytes) + " / " + fmtBytes(intVol.totalBytes)
            : "Unknown";
        ListEntry e; e.label = "Internal Flash"; e.value = pushVal(v);
        append(ListItemRow(a, e));
    }

    if (svc->hasExternal()) {
        auto extVol = svc->externalVolume();
        std::string v = extVol.totalBytes > 0
            ? fmtBytes(extVol.usedBytes) + " / " + fmtBytes(extVol.totalBytes)
            : "Unknown";
        ListEntry e; e.label = "SD Card"; e.value = pushVal(v);
        append(ListItemRow(a, e));
    } else {
        ListEntry e; e.label = "SD Card"; e.value = "Not mounted";
        append(ListItemRow(a, e));
    }

    // ── Per-app list ─────────────────────────────────────────────────────────
    apps_ = svc->allApps();
    if (!apps_.empty()) {
        bool hasExt = svc->hasExternal();
        append(ListSection(a, "Apps"));
        for (size_t i = 0; i < apps_.size(); ++i) {
            auto& app = apps_[i];
            size_t total = app.internalBytes + app.externalBytes;
            const char* locStr = (app.location == StorageLocation::External)
                                     ? "SD" : "Int";
            std::string v = fmtBytes(total) + " \xB7 " + locStr;

            items_.push_back({this, i});
            ListEntry e;
            e.label   = app.displayName.c_str();
            e.value   = pushVal(v);
            e.chevron = app.movable && hasExt;
            if (app.movable && hasExt) {
                e.onPress = onSelect;
                e.user    = &items_.back();
            }
            append(ListItemRow(a, e));
        }
    }

    return View(a, root, { list });
}

} // namespace nema
