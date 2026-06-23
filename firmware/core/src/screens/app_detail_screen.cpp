// Plan 87 Fase 7 — AppDetailScreen: per-app permission + storage + uninstall.
#include "nema/screens/app_detail_screen.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/services/permission_service.h"
#include "nema/services/storage_service.h"
#include "nema/ui/widgets.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/app/app_registry.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

// All @tier(sensitive) capabilities in the system. AppDetailScreen checks
// each one and shows a toggle for any that the app has ever been asked about.
static const char* const kSensitiveCaps[] = {
    "net.wifi.monitor",
    "net.wifi.inject",
    nullptr  // sentinel
};

static const char* capLabel(const char* cap) {
    if (cap == std::string_view("net.wifi.monitor")) return "Wi-Fi Monitor";
    if (cap == std::string_view("net.wifi.inject"))  return "Wi-Fi Inject / Deauth";
    return cap;
}

AppDetailScreen::AppDetailScreen(Runtime& rt)
    : ComponentScreen(rt, 384) {}

void AppDetailScreen::setApp(std::string id, std::string displayName,
                              const uint8_t* iconBitmap,
                              uint8_t iconW, uint8_t iconH) {
    appId_       = std::move(id);
    displayName_ = std::move(displayName);
    iconBitmap_  = iconBitmap;
    iconW_       = iconW;
    iconH_       = iconH;
}

void AppDetailScreen::onResume() {
    scroll_.scrollMain   = 0;
    state_.focus.focused = 0;
    hasStorageInfo_ = false;
    if (auto* svc = rt_.container().resolve<StorageService>()) {
        for (const auto& info : svc->allApps()) {
            if (info.bundleId == appId_) {
                storageInfo_   = info;
                hasStorageInfo_ = true;
                break;
            }
        }
    }
    dirty_ = true;
    rt_.view().requestRedraw();
}

std::string AppDetailScreen::fmtBytes(size_t bytes) {
    char buf[24];
    if (bytes >= 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%zu MB", bytes / (1024 * 1024));
    else if (bytes >= 1024)
        std::snprintf(buf, sizeof(buf), "%zu KB", bytes / 1024);
    else
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    return buf;
}

void AppDetailScreen::onCapToggle(void* u) {
    auto* cr   = static_cast<CapRow*>(u);
    auto* perm = cr->self->rt_.container().resolve<PermissionService>();
    if (!perm) return;
    // Only revoke — re-granting happens organically when the app next requests.
    if (perm->status(cr->self->appId_, cr->cap) == 1)
        perm->revoke(cr->self->appId_, cr->cap);
    cr->self->rt_.view().requestRedraw();
}

void AppDetailScreen::onMove(void* u) {
    auto* self = static_cast<AppDetailScreen*>(u);
    auto* svc  = self->rt_.container().resolve<StorageService>();
    if (!svc || !self->hasStorageInfo_ || !self->storageInfo_.movable) return;
    StorageLocation target =
        (self->storageInfo_.location == StorageLocation::External)
        ? StorageLocation::Internal
        : StorageLocation::External;
    svc->move(self->appId_.c_str(), target);
    self->rt_.view().requestRedraw();
}

void AppDetailScreen::onUninstall(void* u) {
    auto* self = static_cast<AppDetailScreen*>(u);
    self->rt_.apps().uninstall(self->appId_.c_str());
    self->rt_.view().pop();  // go back to app list
}

UiNode* AppDetailScreen::build(NodeArena& a, Runtime& rt) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;
    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    // ── Header row ────────────────────────────────────────────────────────────
    {
        ListEntry e;
        e.label = displayName_.c_str();
        if (iconBitmap_) { e.leftIcon = iconBitmap_; e.iconW = iconW_; e.iconH = iconH_; }
        append(ListItemRow(a, e));
    }

    // ── Permissions ───────────────────────────────────────────────────────────
    auto* perm = rt.container().resolve<PermissionService>();
    bool anyPerm = false;
    int ci = 0;
    for (int i = 0; kSensitiveCaps[i] && ci < 8; ++i) {
        const char* cap = kSensitiveCaps[i];
        uint8_t st = perm ? perm->status(appId_, cap) : 0;
        if (st == 0) continue;  // never asked → don't show
        if (!anyPerm) { append(ListSection(a, "Permissions")); anyPerm = true; }
        capRows_[ci] = {this, cap};
        append(Toggle(a, capLabel(cap), st == 1, onCapToggle, &capRows_[ci]));
        ++ci;
    }
    if (!anyPerm) {
        append(ListSection(a, "Permissions"));
        ListEntry e; e.label = "No permissions requested";
        append(ListItemRow(a, e));
    }

    // ── Storage ───────────────────────────────────────────────────────────────
    append(ListSection(a, "Storage"));
    vals_.clear();
    auto pushVal = [this](const std::string& s) -> const char* {
        vals_.push_back(s);
        return vals_.back().c_str();
    };
    if (hasStorageInfo_) {
        auto* svc = rt.container().resolve<StorageService>();
        bool hasExt = svc && svc->hasExternal();
        size_t total = storageInfo_.internalBytes + storageInfo_.externalBytes;
        const char* locStr = (storageInfo_.location == StorageLocation::External)
                             ? "SD" : "Internal";
        {
            ListEntry e; e.label = "Used"; e.value = pushVal(fmtBytes(total) + " / " + locStr);
            append(ListItemRow(a, e));
        }
        if (storageInfo_.movable && hasExt) {
            ListEntry e;
            e.label   = (storageInfo_.location == StorageLocation::External)
                        ? "Move to Internal" : "Move to SD Card";
            e.chevron = true;
            e.onPress = onMove;
            e.user    = this;
            append(ListItemRow(a, e));
        }
    } else {
        ListEntry e; e.label = "No data";
        append(ListItemRow(a, e));
    }

    // ── Uninstall ─────────────────────────────────────────────────────────────
    append(ListSection(a, "Manage"));
    {
        ListEntry e;
        e.label   = "Uninstall";
        e.chevron = true;
        e.onPress = onUninstall;
        e.user    = this;
        append(ListItemRow(a, e));
    }

    return View(a, root, { list });
}

} // namespace nema
