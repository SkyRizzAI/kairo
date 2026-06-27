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

void AppListScreen::loadInstalledPapps() {
    names_.clear(); ids_.clear(); icons_.clear(); customIcons_.clear();

    for (const auto& m : rt_.apps().list()) {
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

    // Sort all parallel arrays together alphabetically, case-insensitive.
    if (names_.size() > 1) {
        // Build a temporary flat list, sort, then unpack back.
        struct Entry {
            std::string      name;
            std::string      id;
            const IconDef*   icon;
            CustomIcon       customIcon;
        };
        std::vector<Entry> entries;
        entries.reserve(names_.size());
        for (size_t i = 0; i < names_.size(); i++) {
            entries.push_back({std::move(names_[i]), std::move(ids_[i]),
                               icons_[i], customIcons_[i]});
        }
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            size_t n = std::min(a.name.size(), b.name.size());
            for (size_t i = 0; i < n; i++) {
                char ca = (char)std::tolower((unsigned char)a.name[i]);
                char cb = (char)std::tolower((unsigned char)b.name[i]);
                if (ca != cb) return ca < cb;
            }
            return a.name.size() < b.name.size();
        });
        names_.clear(); ids_.clear(); icons_.clear(); customIcons_.clear();
        for (auto& e : entries) {
            names_.push_back(std::move(e.name));
            ids_.push_back(std::move(e.id));
            icons_.push_back(e.icon);
            customIcons_.push_back(e.customIcon);
        }
    }

    // Placeholder when no apps are installed.
    if (names_.empty()) {
        names_.push_back("No apps installed");
        ids_.push_back("");
        icons_.push_back(nullptr);
        customIcons_.push_back({nullptr, 0, 0});
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void AppListScreen::onResume() {
    // Re-sync the registry with the .papp folders on disk every time the launcher
    // opens (lazy refresh, Flipper-style): newly copied apps appear, deleted ones
    // disappear, and updated ones (changed signature) are reinstalled — all
    // without a reboot. Then rebuild the on-screen list from the fresh registry.
    nema::loadInstalledPapps(rt_);
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

    bool selectable = !self->ids_[index].empty();

    ListEntry e;
    e.label   = self->names_[index].c_str();
    e.chevron = selectable;

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
            rt_.view().goBack();
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

void AppListScreen::activateFocused() {
    int i = vlist_.focusedIndex;
    if (i < 0 || i >= (int)ids_.size() || ids_[i].empty()) return;

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
    // Hold-OK in Launch mode uses launchDetail_; in Detail mode falls back to detailScreen_.
    AppDetailScreen* detail = launchDetail_ ? launchDetail_ : detailScreen_;
    if (!detail) return;
    int i = vlist_.focusedIndex;
    if (i < 0 || i >= (int)ids_.size() || ids_[i].empty()) return;
    const auto& ci = customIcons_[i];
    detail->setApp(ids_[i], names_[i], ci.bitmap, ci.w, ci.h);
    rt_.view().push(*detail);
}

} // namespace nema
