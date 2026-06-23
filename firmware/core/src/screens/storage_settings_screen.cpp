// Plan 83 Fase 5 — StorageSettingsScreen: volume overview only.
// Per-app detail (permissions + move + uninstall) moved to AppDetailScreen (Plan 87 Fase 7).
// Plan 89: async load via TaskRunner (Fase 3) + SD card info + eject (Fase 4).
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
    cached_.ready = false;
    dirty_ = true;  // show "Loading…" immediately
    rt_.view().requestRedraw();

    auto* svc = rt_.container().resolve<StorageService>();
    if (!svc) return;

    // Kick an async task so the VFS scan doesn't block the UI thread.
    // Worker writes to cached_; done callback sets ready = true and redraws.
    rt_.tasks().submit(
        [svc, this]() {
            cached_.intVol = svc->internalVolume();
            cached_.sdInfo = svc->sdCardInfo();
            if (cached_.sdInfo.mounted) cached_.extVol = svc->externalVolume();
        },
        [this]() {
            cached_.ready = true;
            vals_.clear();
            dirty_ = true;
            rt_.view().requestRedraw();
        }
    );
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

static std::string fmtBytes64(uint64_t bytes) {
    char buf[24];
    if (bytes >= (uint64_t)1024 * 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%llu GB", (unsigned long long)(bytes / (1024ULL * 1024 * 1024)));
    else if (bytes >= 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%llu MB", (unsigned long long)(bytes / (1024 * 1024)));
    else if (bytes >= 1024)
        std::snprintf(buf, sizeof(buf), "%llu KB", (unsigned long long)(bytes / 1024));
    else
        std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    return buf;
}

void StorageSettingsScreen::onEjectSd(void* u) {
    auto* self = static_cast<StorageSettingsScreen*>(u);
    auto* svc  = self->rt_.container().resolve<StorageService>();
    if (svc) svc->ejectSd();
    // Kick a fresh async load to reflect the ejected state.
    self->onResume();
}

aether::ui::UiNode* StorageSettingsScreen::build(NodeArena& a, Runtime& /*rt*/) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;
    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    // vals_ owns all formatted value strings — ListEntry holds const char* into them.
    vals_.clear();
    auto pushVal = [&](const std::string& s) -> const char* {
        vals_.push_back(s);
        return vals_.back().c_str();
    };

    append(ListSection(a, "Storage"));

    if (!cached_.ready) {
        ListEntry e; e.label = "Loading…";
        append(ListItemRow(a, e));
        return View(a, root, { list });
    }

    // ── Internal flash ────────────────────────────────────────────────────────
    {
        const auto& v = cached_.intVol;
        std::string val = v.totalBytes > 0
            ? fmtBytes(v.usedBytes) + " / " + fmtBytes(v.totalBytes)
            : fmtBytes(v.usedBytes) + " used";
        ListEntry e; e.label = "Internal Flash"; e.value = pushVal(val);
        append(ListItemRow(a, e));
    }

    // ── SD card ───────────────────────────────────────────────────────────────
    const auto& sd = cached_.sdInfo;
    if (!sd.mounted) {
        ListEntry e; e.label = "SD Card"; e.value = "Not mounted";
        append(ListItemRow(a, e));
    } else {
        // Show capacity if the backend reported it.
        std::string sdVal;
        if (sd.totalBytes > 0) {
            uint64_t used = sd.totalBytes > sd.freeBytes
                            ? sd.totalBytes - sd.freeBytes : 0;
            sdVal = fmtBytes64(used) + " / " + fmtBytes64(sd.totalBytes);
        } else {
            // Fall back to scanned used-bytes only.
            sdVal = fmtBytes(cached_.extVol.usedBytes) + " used";
        }
        {
            ListEntry e; e.label = "SD Card"; e.value = pushVal(sdVal);
            append(ListItemRow(a, e));
        }
        // Eject action row.
        {
            ListEntry e;
            e.label   = "Eject SD Card";
            e.chevron = true;
            e.onPress = onEjectSd;
            e.user    = this;
            append(ListItemRow(a, e));
        }
    }

    return View(a, root, { list });
}

} // namespace nema
