// Plan 60 — AppListScreen: standardised-UI list of installed apps.
// Plan 52/53 — per-app icon from icon_pack or bundled bitmap (Plan 84).
// Plan 79+  — VirtualList with app-managed focus + alphabetical sorting.
#include "nema/screens/app_list_screen.h"
#include "nema/screens/app_detail_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/virtual_list.h"
#include "nema/ui/icon_pack.h"
#include "nema/app/app_registry.h"
#include "nema/app/app_manifest.h"
#include "nema/app/papp_installer.h"
#include "nema/ui/view_dispatcher.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace nema {

using namespace aether::ui;

// Slot height = ListItemRow height (12) + 2px inter-row gap, matching ListContainer's
// gap=2. VirtualList can't use Style::gap (it would desync the spacer math), so the gap is
// baked into the slot height and added as a 2px bottom margin on each row (see renderAppItem).
static constexpr uint16_t kItemH = 14;

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

// ── Data loading ──────────────────────────────────────────────────────────────

// Case-insensitive name ordering, matching the previous alphabetical sort.
static bool nameLess(const std::string& a, const std::string& b) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        char ca = (char)std::tolower((unsigned char)a[i]);
        char cb = (char)std::tolower((unsigned char)b[i]);
        if (ca != cb) return ca < cb;
    }
    return a.size() < b.size();
}

// The Launchpad folder an app belongs to: its category, unless that is empty /
// "Apps" / "System" → "" (top-level, no folder).
static std::string folderOf(const AppManifest& m) {
    if (!m.category) return "";
    if (std::strcmp(m.category, "Apps")   == 0) return "";
    if (std::strcmp(m.category, "System") == 0) return "";
    return std::string(m.category);
}

void AppListScreen::loadInstalledPapps() {
    names_.clear(); ids_.clear(); icons_.clear(); customIcons_.clear();
    isFolder_.clear(); isBack_.clear();

    // A flat app row gathered from the registry, ready to be sorted/emitted.
    struct AppRow {
        std::string    name;
        std::string    id;
        const IconDef* icon;
        CustomIcon     customIcon;
    };
    auto makeRow = [](const AppManifest& m) -> AppRow {
        AppRow r{m.name, m.id, nullptr, {nullptr, 0, 0}};
        if (m.iconBitmap && m.iconW && m.iconH)
            r.customIcon = {m.iconBitmap, m.iconW, m.iconH};
        else
            r.icon = iconForApp(m);
        return r;
    };
    auto pushApp = [&](AppRow& r) {
        names_.push_back(std::move(r.name));
        ids_.push_back(std::move(r.id));
        icons_.push_back(r.icon);
        customIcons_.push_back(r.customIcon);
        isFolder_.push_back(false);
        isBack_.push_back(false);
    };
    auto pushFolder = [&](const std::string& folder) {
        names_.push_back(folder);
        ids_.push_back("");                       // folders aren't launchable
        // Distinct folder icon (matches the file browser) so a category folder is
        // visually different from an app that falls back to the generic apps icon.
        icons_.push_back(findIcon("file.folder"));
        customIcons_.push_back({nullptr, 0, 0});
        isFolder_.push_back(true);
        isBack_.push_back(false);
    };

    if (curFolder_.empty()) {
        // ── Root view: distinct folders (alpha), then top-level apps (alpha). ──
        std::vector<std::string> folders;
        std::vector<AppRow>      topApps;
        for (const auto& m : rt_.apps().list()) {
            if (m.type != AppType::App) continue;
            if (m.category && std::strcmp(m.category, "System") == 0) continue;
            std::string f = folderOf(m);
            if (f.empty()) {
                topApps.push_back(makeRow(m));
            } else if (std::find(folders.begin(), folders.end(), f) == folders.end()) {
                folders.push_back(f);
            }
        }
        std::sort(folders.begin(), folders.end(), nameLess);
        std::sort(topApps.begin(), topApps.end(),
                  [](const AppRow& a, const AppRow& b) { return nameLess(a.name, b.name); });
        for (const auto& f : folders) pushFolder(f);
        for (auto& r : topApps)       pushApp(r);
    } else {
        // ── Folder view: ".." back row, then the apps inside curFolder_ (alpha). ──
        names_.push_back("Back");
        ids_.push_back("");
        icons_.push_back(findIcon("nav.up"));
        customIcons_.push_back({nullptr, 0, 0});
        isFolder_.push_back(false);
        isBack_.push_back(true);

        std::vector<AppRow> folderApps;
        for (const auto& m : rt_.apps().list()) {
            if (m.type != AppType::App) continue;
            if (m.category && std::strcmp(m.category, "System") == 0) continue;
            if (folderOf(m) == curFolder_) folderApps.push_back(makeRow(m));
        }
        std::sort(folderApps.begin(), folderApps.end(),
                  [](const AppRow& a, const AppRow& b) { return nameLess(a.name, b.name); });
        for (auto& r : folderApps) pushApp(r);
    }

    // Placeholder when there is nothing to show (only at the root — a folder view
    // always has at least the back row).
    if (names_.empty()) {
        names_.push_back("No apps installed");
        ids_.push_back("");
        icons_.push_back(nullptr);
        customIcons_.push_back({nullptr, 0, 0});
        isFolder_.push_back(false);
        isBack_.push_back(false);
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void AppListScreen::onResume() {
    // Re-sync the registry with the .papp folders on disk every time the launcher
    // opens (lazy refresh, Flipper-style): newly copied apps appear, deleted ones
    // disappear, and updated ones (changed signature) are reinstalled — all
    // without a reboot. Then rebuild the on-screen list from the fresh registry.
    nema::loadInstalledPapps(rt_);
    curFolder_.clear();              // always reopen at the root
    loadInstalledPapps();
    vlist_.scrollMain   = 0;
    vlist_.focusedIndex = 0;
    ComponentScreen::onResume();
}

// ── VirtualList renderItem callback ───────────────────────────────────────────

// `focused` is true when index == vst.focusedIndex. VirtualList forces all
// items focusable=false (out of ComponentRuntime's focus tree), so selectBox
// does not fire here. Chevron signals that the row is selectable instead.
UiNode* AppListScreen::renderAppItem(NodeArena& a, int index,
                                      bool focused, void* userdata) {
    auto* self = static_cast<AppListScreen*>(userdata);
    if (index < 0 || index >= (int)self->names_.size()) return nullptr;

    bool isFolder = self->isFolder_[index];
    bool isBack   = self->isBack_[index];
    // Folders and the back row are navigable even though they have no app id.
    bool selectable = isFolder || isBack || !self->ids_[index].empty();

    ListEntry e;
    e.label   = self->names_[index].c_str();
    // A chevron signals "drill in" (apps + folders); the back row points up, not in.
    e.chevron = selectable && !isBack;

    const auto& ci = self->customIcons_[index];
    if (ci.bitmap) {
        e.leftIcon = ci.bitmap; e.iconW = ci.w; e.iconH = ci.h;
    } else if (self->icons_[index]) {
        e.leftIcon = self->icons_[index]->bitmap;
        e.iconW    = self->icons_[index]->w;
        e.iconH    = self->icons_[index]->h;
    }

    UiNode* row = ListItemRow(a, e);
    if (row) {
        row->selfHighlight = focused;   // VirtualList items aren't in the focus tree
        row->style.mb      = 2;          // inter-row gap → fills kItemH (12 row + 2)
    }
    return row;
}

// ── Build ─────────────────────────────────────────────────────────────────────

UiNode* AppListScreen::build(NodeArena& a, Runtime& /*rt*/) {
    Style root;
    root.dir     = FlexDir::Col;
    root.flexGrow = 1;
    root.align   = Align::Stretch;

    // 2px inset to match ListContainer (every other settings list). The row gap comes from
    // kItemH/mb, not Style::gap (which would break VirtualList's spacer math).
    Style ls; ls.padding = 2;
    UiNode* list = VirtualList(a, vlist_, (int)names_.size(), kItemH,
                               renderAppItem, this, ls);

    return View(a, root, { list });
}

// ── Input ─────────────────────────────────────────────────────────────────────

void AppListScreen::onAction(input::Action a) {
    using A = input::Action;
    switch (a) {
        case A::Prev:
        case A::AdjustDown:
            if (vlist_.moveFocus(-1)) { dirty_ = true; requestRedraw(); }
            break;
        case A::Next:
        case A::AdjustUp:
            if (vlist_.moveFocus(+1)) { dirty_ = true; requestRedraw(); }
            break;
        case A::Activate:
            activateFocused();
            break;
        case A::Menu:
            openDetailForFocused();
            break;
        case A::Back:
            // Inside a folder, Back goes up to the root instead of exiting the
            // launcher; at the root, the existing exit/goBack behaviour.
            if (!curFolder_.empty()) enterFolder("");
            else                     rt_.view().goBack();
            break;
        default:
            break;
    }
}

void AppListScreen::tick(uint64_t nowMs) {
    ComponentScreen::tick(nowMs);
    // VirtualList items are focusable=false so state_.focus.count stays 0 —
    // ComponentScreen::tick()'s marquee guard never fires. Drive it here instead.
    if (!names_.empty() && (nowMs - lastMarqueeMs_) >= 66) {
        lastMarqueeMs_ = nowMs;
        requestRedraw();
    }
}

// ── Actions ───────────────────────────────────────────────────────────────────

void AppListScreen::enterFolder(const std::string& folder) {
    curFolder_ = folder;
    loadInstalledPapps();
    vlist_.scrollMain   = 0;
    vlist_.focusedIndex = 0;
    dirty_ = true;
    requestRedraw();
}

void AppListScreen::activateFocused() {
    int i = vlist_.focusedIndex;
    if (i < 0 || i >= (int)ids_.size()) return;

    // Launchpad navigation: back row → root, folder row → drill in.
    if (isBack_[i])   { enterFolder("");          return; }
    if (isFolder_[i]) { enterFolder(names_[i]);   return; }
    if (ids_[i].empty()) return;   // placeholder ("No apps installed")

    if (detailScreen_) {
        // Detail mode (Settings → Apps): push the detail screen.
        const auto& ci = customIcons_[i];
        detailScreen_->setApp(ids_[i], names_[i], ci.bitmap, ci.w, ci.h);
        rt_.view().push(*detailScreen_);
    } else {
        // Launch mode (home launcher): start the app directly.
        rt_.apps().launch(ids_[i].c_str());
    }
}

void AppListScreen::openDetailForFocused() {
    int i = vlist_.focusedIndex;
    if (i < 0 || i >= (int)ids_.size()) return;

    // Folder/back rows have no detail — navigate instead.
    if (isBack_[i])   { enterFolder("");          return; }
    if (isFolder_[i]) { enterFolder(names_[i]);   return; }
    if (ids_[i].empty()) return;   // placeholder

    // Hold-OK in Launch mode uses launchDetail_; in Detail mode falls back to detailScreen_.
    AppDetailScreen* detail = launchDetail_ ? launchDetail_ : detailScreen_;
    if (!detail) return;
    const auto& ci = customIcons_[i];
    detail->setApp(ids_[i], names_[i], ci.bitmap, ci.w, ci.h);
    rt_.view().push(*detail);
}

} // namespace nema
