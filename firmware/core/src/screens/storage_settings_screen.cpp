// Plan 83 Fase 5 — StorageSettingsScreen: volume overview only.
// Per-app detail (permissions + move + uninstall) moved to AppDetailScreen (Plan 87 Fase 7).
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


aether::ui::UiNode* StorageSettingsScreen::build(NodeArena& a, Runtime& rt) {
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
            : fmtBytes(intVol.usedBytes) + " used";
        ListEntry e; e.label = "Internal Flash"; e.value = pushVal(v);
        append(ListItemRow(a, e));
    }

    if (svc->hasExternal()) {
        auto extVol = svc->externalVolume();
        std::string v = extVol.totalBytes > 0
            ? fmtBytes(extVol.usedBytes) + " / " + fmtBytes(extVol.totalBytes)
            : fmtBytes(extVol.usedBytes) + " used";
        ListEntry e; e.label = "SD Card"; e.value = pushVal(v);
        append(ListItemRow(a, e));
    } else {
        ListEntry e; e.label = "SD Card"; e.value = "Not mounted";
        append(ListItemRow(a, e));
    }

    // Per-app storage + permissions → Settings → Apps → [app] (AppDetailScreen).

    return View(a, root, { list });
}

} // namespace nema
