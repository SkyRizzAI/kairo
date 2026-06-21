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

    // ── Volume overview ──────────────────────────────────────────────────────
    append(ListSection(a, "Storage"));

    auto intVol = svc->internalVolume();
    {
        char val[32];
        if (intVol.totalBytes > 0)
            std::snprintf(val, sizeof(val), "%s / %s",
                fmtBytes(intVol.usedBytes).c_str(), fmtBytes(intVol.totalBytes).c_str());
        else
            std::snprintf(val, sizeof(val), "Unknown");
        ListEntry e; e.label = "Internal Flash"; e.value = val;
        append(ListItemRow(a, e));
    }

    if (svc->hasExternal()) {
        auto extVol = svc->externalVolume();
        char val[32];
        if (extVol.totalBytes > 0)
            std::snprintf(val, sizeof(val), "%s / %s",
                fmtBytes(extVol.usedBytes).c_str(), fmtBytes(extVol.totalBytes).c_str());
        else
            std::snprintf(val, sizeof(val), "Unknown");
        ListEntry e; e.label = "SD Card"; e.value = val;
        append(ListItemRow(a, e));
    } else {
        ListEntry e; e.label = "SD Card"; e.value = "Not mounted";
        append(ListItemRow(a, e));
    }

    // ── Per-app list ─────────────────────────────────────────────────────────
    apps_ = svc->allApps();
    if (!apps_.empty()) {
        append(ListSection(a, "Apps"));
        for (size_t i = 0; i < apps_.size(); ++i) {
            auto& app = apps_[i];
            size_t total = app.internalBytes + app.externalBytes;
            const char* locStr = (app.location == StorageLocation::External)
                                     ? "SD Card" : "Internal";
            char val[32];
            std::snprintf(val, sizeof(val), "%s · %s",
                fmtBytes(total).c_str(), locStr);

            items_.push_back({this, i});
            ListEntry e;
            e.label   = app.displayName.c_str();
            e.value   = val;
            e.chevron = app.movable && svc->hasExternal();
            if (app.movable && svc->hasExternal()) {
                e.onPress = onSelect;
                e.user    = &items_.back();
            }
            append(ListItemRow(a, e));
        }
    }

    return View(a, root, { list });
}

} // namespace nema
