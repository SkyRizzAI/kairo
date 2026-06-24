// Plan 81 — LauncherScreen implementation.
// Plan 82 Phase 4+6 — T2 animated icons + BadUSB as System entry.
#include "aether/screens/launcher_screen.h"
#include "aether/shell/shell_factory.h"
#include "nema/assets/system_anims.h"
#include "nema/ui/canvas.h"
#include "nema/ui/icon_pack.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/input/input_action.h"
#include "nema/app/app_registry.h"
#include "nema/runtime.h"
#include "nema/config/config_store.h"
#include <cstdio>
#include <cstring>

namespace nema {

LauncherScreen::LauncherScreen(Runtime& rt)
    : ComponentScreen(rt),
      appList_(rt), appDetail_(rt), files_(rt), dolphin_(rt), logs_(rt), settings_(rt) {
    appList_.setLaunchDetailScreen(&appDetail_);
}

// One entry record keyed by label, animation, and fallback icon handle.
struct EntryDef {
    const char*                    label;
    const nema::anim::Animation*   anim;        // T2 icon animation (may be null)
    const char*                    iconHandle;  // icon_pack fallback
};

static const EntryDef kEntries[] = {
    { "Apps",     &nema::assets::animIconApps,     "feature.apps"     },
    { "Files",    nullptr,                          "file.folder"      },
    { "Dolphin",  nullptr,                          "action.info"      },
    { "Logs",     nullptr,                          "file.file"        },
    { "Settings", &nema::assets::animIconSettings,  "feature.settings" },
    { "BadUSB",   &nema::assets::animIconBadusb,    nullptr            },
};
static constexpr int kEntryCount = (int)(sizeof(kEntries) / sizeof(kEntries[0]));

void LauncherScreen::buildEntries() {
    entries_.clear();
    players_.clear();

    for (int i = 0; i < kEntryCount; i++) {
        const EntryDef& def = kEntries[i];
        shell::LauncherEntry e;
        e.label = def.label;

        if (def.anim) {
            // T2 animated icon: create a player, wire its first frame as static fallback.
            auto p = std::make_unique<nema::anim::AnimationPlayer>(*def.anim);
            p->start();
            e.player = p.get();
            // Static fallback = first unique frame
            if (def.anim->frameCount > 0) {
                e.icon  = def.anim->frames[0].bitmap;
                e.iconW = (uint8_t)def.anim->frames[0].width;
                e.iconH = (uint8_t)def.anim->frames[0].height;
            }
            players_.push_back(std::move(p));
        } else if (def.iconHandle) {
            if (auto* d = aether::ui::findIcon(def.iconHandle)) {
                e.icon = d->bitmap; e.iconW = d->w; e.iconH = d->h;
            }
            players_.push_back(nullptr);
        } else {
            players_.push_back(nullptr);
        }

        entries_.push_back(e);
    }
}

void LauncherScreen::activate(int i) {
    switch (i) {
        case 0: rt_.view().navigate(appList_);  break;
        case 1: rt_.view().navigate(files_);    break;
        case 2: rt_.view().navigate(dolphin_);  break;
        case 3: rt_.view().navigate(logs_);     break;
        case 4: rt_.view().navigate(settings_); break;
        case 5: rt_.apps().launch("com.palanu.badusb"); break;
        default: break;
    }
}

void LauncherScreen::onResume() {
    ComponentScreen::onResume();
    buildEntries();

    std::string name = rt_.config().getString("aether", "launcher", shell::kDefaultLauncher);
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

void LauncherScreen::tick(uint64_t nowMs) {
    ComponentScreen::tick(nowMs);
    // Tick all T2 icon players; request a redraw if any frame advanced.
    bool dirty = false;
    for (auto& p : players_) {
        if (p && p->tick((uint32_t)nowMs)) dirty = true;
    }
    if (dirty) requestRedraw();
}

void LauncherScreen::onAction(input::Action a) {
    using input::Action;
    int n = (int)entries_.size();
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
    // Sync e.icon to the player's current frame before handing off to the skin.
    for (int i = 0; i < (int)entries_.size(); i++) {
        auto& p = players_[i];
        if (p && p->currentFrameData()) {
            entries_[i].icon  = p->currentFrameData();
            entries_[i].iconW = (uint8_t)p->width();
            entries_[i].iconH = (uint8_t)p->height();
        }
    }

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
