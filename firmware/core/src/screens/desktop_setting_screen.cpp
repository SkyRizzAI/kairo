// Plan 81 — DesktopSettingScreen implementation.
#include "nema/screens/desktop_setting_screen.h"
#include "aether/shell/desktop_theme.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/config/config_store.h"
#include "nema/hal/filesystem.h"
#include <cstring>
#include <string>
#include <vector>

namespace nema {

using namespace aether::ui;

DesktopSettingScreen::DesktopSettingScreen(Runtime& rt) : ComponentScreen(rt) {}

int DesktopSettingScreen::findFitIdx() const {
    std::string cur = rt_.config().getString("desktop", "fit",
                                             shell::fitName(shell::FitMode::Fit));
    return (int)shell::fitFromName(cur.c_str(), shell::FitMode::Fit);
}
int DesktopSettingScreen::findAnchorIdx() const {
    std::string cur = rt_.config().getString("desktop", "anchor",
                                             shell::anchorName(shell::Anchor::Center));
    return (int)shell::anchorFromName(cur.c_str(), shell::Anchor::Center);
}

// Discover *.panim under /system/assets/anims and sync the current selection from
// config "desktop"/"anim". Always leaves at least one entry ("laptop" fallback).
void DesktopSettingScreen::scanWallpapers() {
    wallCount_ = 0;
    if (rt_.fs()) {
        std::vector<nema::FsEntry> entries;
        if (rt_.fs()->list("/system/assets/anims", entries)) {
            const std::string ext = ".panim";
            for (const auto& e : entries) {
                if (e.isDir || wallCount_ >= kMaxWalls) continue;
                const std::string& nm = e.name;
                if (nm.size() <= ext.size() ||
                    nm.compare(nm.size() - ext.size(), ext.size(), ext) != 0) continue;
                std::string base = nm.substr(0, nm.size() - ext.size());
                std::strncpy(wallNames_[wallCount_], base.c_str(), kWallNameLen - 1);
                wallNames_[wallCount_][kWallNameLen - 1] = '\0';
                wallCount_++;
            }
        }
    }
    if (wallCount_ == 0) {   // no FS / empty dir → at least offer the default
        std::strncpy(wallNames_[0], "laptop", kWallNameLen - 1);
        wallNames_[0][kWallNameLen - 1] = '\0';
        wallCount_ = 1;
    }
    std::string cur = rt_.config().getString("desktop", "anim", wallNames_[0]);
    wallIdx_ = 0;
    for (int i = 0; i < wallCount_; i++)
        if (cur == wallNames_[i]) { wallIdx_ = i; break; }
}

void DesktopSettingScreen::cycleFit(int dir) {
    fitIdx_ = (fitIdx_ + dir + shell::kFitCount) % shell::kFitCount;
    rt_.config().setString("desktop", "fit", shell::fitNames()[fitIdx_]);
    rt_.view().requestRedraw();
}
void DesktopSettingScreen::cycleAnchor(int dir) {
    anchorIdx_ = (anchorIdx_ + dir + shell::kAnchorCount) % shell::kAnchorCount;
    rt_.config().setString("desktop", "anchor", shell::anchorNames()[anchorIdx_]);
    rt_.view().requestRedraw();
}
void DesktopSettingScreen::cycleWall(int dir) {
    if (wallCount_ <= 0) return;
    wallIdx_ = (wallIdx_ + dir + wallCount_) % wallCount_;
    rt_.config().setString("desktop", "anim", wallNames_[wallIdx_]);   // livewall reads this
    rt_.view().requestRedraw();
}

void DesktopSettingScreen::fitAdj(void* u, int dir)    { static_cast<DesktopSettingScreen*>(u)->cycleFit(dir); }
void DesktopSettingScreen::anchorAdj(void* u, int dir) { static_cast<DesktopSettingScreen*>(u)->cycleAnchor(dir); }
void DesktopSettingScreen::wallAdj(void* u, int dir)   { static_cast<DesktopSettingScreen*>(u)->cycleWall(dir); }

void DesktopSettingScreen::onResume() {
    fitIdx_    = findFitIdx();
    anchorIdx_ = findAnchorIdx();
    scanWallpapers();          // discover anims + sync wallIdx_ from config
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    requestRedraw();
}

UiNode* DesktopSettingScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    auto input = [&](const char* label, const char* value, void (*adj)(void*, int)) {
        ListInput e;
        e.label = label; e.value = value;
        e.onAdjust = adj; e.user = this;
        return ListInputRow(a, e);
    };

    return View(a, root, {
        ListContainer(a, scroll_, {
            ListSection(a, "Wallpaper"),
            input("Wallpaper", wallNames_[wallIdx_],            wallAdj),
            input("Fit",       shell::fitNames()[fitIdx_],      fitAdj),
            input("Anchor",    shell::anchorNames()[anchorIdx_], anchorAdj),
        }),
    });
}

} // namespace nema
