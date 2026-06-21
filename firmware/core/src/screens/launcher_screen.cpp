// Plan 81 — LauncherScreen implementation.
#include "nema/screens/launcher_screen.h"
#include "nema/shell/shell_factory.h"
#include "nema/ui/canvas.h"
#include "nema/ui/icon_pack.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/input/input_action.h"
#include "nema/runtime.h"
#include "nema/config/config_store.h"
#include <cstdio>
#include <cstring>

namespace nema {

LauncherScreen::LauncherScreen(Runtime& rt)
    : ComponentScreen(rt),
      appList_(rt), files_(rt), dolphin_(rt), logs_(rt), settings_(rt) {}

// Fixed system entries — same set every skin draws; index drives activate().
void LauncherScreen::buildEntries() {
    entries_.clear();
    auto add = [&](const char* label, const char* iconHandle) {
        shell::LauncherEntry e;
        e.label = label;
        if (auto* d = aether::ui::findIcon(iconHandle)) {
            e.icon = d->bitmap; e.iconW = d->w; e.iconH = d->h;
        }
        entries_.push_back(e);
    };
    add("Apps",     "feature.apps");
    add("Files",    "file.folder");
    add("Dolphin",  "action.info");
    add("Logs",     "file.file");
    add("Settings", "feature.settings");
}

void LauncherScreen::activate(int i) {
    switch (i) {
        case 0: rt_.view().navigate(appList_);  break;
        case 1: rt_.view().navigate(files_);    break;
        case 2: rt_.view().navigate(dolphin_);  break;
        case 3: rt_.view().navigate(logs_);     break;
        case 4: rt_.view().navigate(settings_); break;
        default: break;
    }
}

void LauncherScreen::onResume() {
    ComponentScreen::onResume();
    buildEntries();

    std::string name = rt_.config().getString("display", "launcher", shell::kDefaultLauncher);
    if (!theme_ || name != theme_->name())
        theme_ = shell::makeLauncher(name.c_str());

    // Banner title (PlayStation skin): device/profile name, else PALANU.
    std::string dev = rt_.config().getString("profile", "name", "PALANU");
    std::snprintf(title_, sizeof(title_), "%s", dev.c_str());

    int n = (int)entries_.size();
    if (cursor_ >= n) cursor_ = n - 1;
    if (cursor_ < 0)  cursor_ = 0;
    requestRedraw();
}

void LauncherScreen::onAction(input::Action a) {
    using input::Action;
    int n = (int)entries_.size();
    // Linear nav: every directional intent moves ±1 in reading order, so the menu
    // works identically on every board (carousel and grid alike) — the skin's
    // columns() only affects layout, not traversal.
    switch (a) {
        case Action::Prev:
        case Action::AdjustDown:
            if (cursor_ > 0) { cursor_--; requestRedraw(); }
            break;
        case Action::Next:
        case Action::AdjustUp:
            if (cursor_ < n - 1) { cursor_++; requestRedraw(); }
            break;
        case Action::Activate:
            activate(cursor_);
            break;
        case Action::Back:
            rt_.view().goBack();   // reveal the Desktop
            break;
        default:
            break;
    }
}

void LauncherScreen::draw(Canvas& c) {
    shell::LauncherModel m;
    m.title = title_;
    m.items = entries_.data();
    m.count = (int)entries_.size();
    if (theme_) theme_->draw(c, m, cursor_);
}

aether::ui::UiNode* LauncherScreen::build(aether::ui::NodeArena&, Runtime&) {
    return nullptr;   // skin paints directly in draw()
}

} // namespace nema
