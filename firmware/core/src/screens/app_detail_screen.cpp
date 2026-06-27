// Plan 87 Fase 7 — AppDetailScreen: per-app permission + storage + uninstall.
// Plan 89 Fase 3: storage info loaded asynchronously via TaskRunner.
#include "nema/screens/app_detail_screen.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/services/permission_service.h"
#include "nema/services/storage_service.h"
#include "nema/ui/widgets.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/app/app_registry.h"
#include "host/sensitive_caps.gen.h"   // kSensitiveCaps[] — generated from the IDL
#include <cstdio>

namespace nema {

using namespace aether::ui;

// The sensitive-capability catalog (kSensitiveCaps / kSensitiveCapCount) is generated
// from the IDL @tier(sensitive) annotations (see host/sensitive_caps.gen.h) — never
// hand-maintained here, so it can't drift from the real permission set.

AppDetailScreen::AppDetailScreen(Runtime& rt)
    : ComponentScreen(rt, 384), confirm_(rt) {}

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
    hasStorageInfo_  = false;
    loadingStorage_  = true;
    dirty_ = true;  // show "Loading…" in Storage section immediately
    rt_.view().requestRedraw();

    auto* svc = rt_.container().resolve<StorageService>();
    if (!svc) { loadingStorage_ = false; return; }

    // allApps() walks the VFS — kick it off the UI thread.
    std::string id = appId_;  // capture by value (string is safe across threads)
    rt_.tasks().submit(
        [svc, id, this]() {
            for (const auto& info : svc->allApps()) {
                if (info.bundleId == id) {
                    storageInfo_    = info;
                    hasStorageInfo_ = true;
                    break;
                }
            }
        },
        [this]() {
            loadingStorage_ = false;
            dirty_ = true;
            rt_.view().requestRedraw();
        }
    );
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
    uint8_t cur = perm->status(cr->self->appId_, cr->cap);
    if (cur == 1)
        perm->revoke(cr->self->appId_, cr->cap);  // granted → revoke (resets to not-asked)
    else
        perm->grant(cr->self->appId_, cr->cap);   // denied → grant directly from Settings
    cr->self->dirty_ = true;
    cr->self->rt_.view().requestRedraw();
}

void AppDetailScreen::onResetPerms(void* u) {
    auto* self = static_cast<AppDetailScreen*>(u);
    self->confirm_.setup("Reset Permissions", "Remove all permission\ngrants?",
                         "Reset", doResetPerms, self, /*danger=*/true);
    self->rt_.view().push(self->confirm_);
}
void AppDetailScreen::doResetPerms(void* u) {
    auto* self = static_cast<AppDetailScreen*>(u);
    self->rt_.view().goBack();   // pop the modal
    auto* perm = self->rt_.container().resolve<PermissionService>();
    if (!perm) return;
    for (int i = 0; i < kSensitiveCapCount; ++i)
        perm->revoke(self->appId_, kSensitiveCaps[i].id);
    self->markDirty();
}

void AppDetailScreen::onMove(void* u) {
    auto* self = static_cast<AppDetailScreen*>(u);
    auto* svc  = self->rt_.container().resolve<StorageService>();
    if (!svc || !self->hasStorageInfo_ || !self->storageInfo_.movable) return;
    StorageLocation target =
        (self->storageInfo_.location == StorageLocation::External)
        ? StorageLocation::Internal
        : StorageLocation::External;
    std::string id = self->appId_;
    self->runBusy("Moving…",
                  [svc, id, target] { svc->move(id.c_str(), target); },
                  [self] { self->onResume(); });  // reload storage info to reflect new location
}

void AppDetailScreen::onUninstall(void* u) {
    auto* self = static_cast<AppDetailScreen*>(u);
    std::snprintf(self->confirmBody_, sizeof(self->confirmBody_),
                  "Delete \"%s\" and\nits data?", self->displayName_.c_str());
    self->confirm_.setup("Uninstall App", self->confirmBody_, "Uninstall",
                         doUninstall, self, /*danger=*/true);
    self->rt_.view().push(self->confirm_);
}
void AppDetailScreen::doUninstall(void* u) {
    auto* self = static_cast<AppDetailScreen*>(u);
    self->rt_.view().goBack();   // pop the modal
    auto* apps = &self->rt_.apps();
    std::string id = self->appId_;
    self->runBusy("Uninstalling…",
                  [apps, id] { apps->uninstall(id.c_str()); },
                  [self] { self->rt_.view().pop(); });  // go back to app list
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
    for (int i = 0; i < kSensitiveCapCount && ci < 8; ++i) {
        const char* cap = kSensitiveCaps[i].id;
        uint8_t st = perm ? perm->status(appId_, cap) : 0;
        if (st == 0) continue;  // never asked → don't show
        if (!anyPerm) { append(ListSection(a, "Permissions")); anyPerm = true; }
        capRows_[ci] = {this, cap};
        append(SwitchRow(a, kSensitiveCaps[i].label, st == 1, onCapToggle, &capRows_[ci]));
        ++ci;
    }
    if (anyPerm) {
        ListEntry re; re.label = "Reset All Permissions"; re.onPress = onResetPerms; re.user = this;
        append(ListItemRow(a, re));
    } else {
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
    if (loadingStorage_) {
        // Non-focusable info row (no onPress) with a spinner accessory — the user
        // can't highlight it; it just signals work in progress.
        ListEntry e; e.label = "Loading…"; e.valueNode = Spinner(a, 11);
        append(ListItemRow(a, e));
    } else if (hasStorageInfo_) {
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
        e.onPress = onUninstall;
        e.user    = this;
        append(ListItemRow(a, e));
    }

    return View(a, root, { list });
}

} // namespace nema
