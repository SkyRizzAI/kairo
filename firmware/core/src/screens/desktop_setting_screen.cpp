// Plan 81 — DesktopSettingScreen implementation.
#include "nema/screens/desktop_setting_screen.h"
#include "aether/shell/desktop_theme.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/config/config_store.h"
#include <string>

namespace nema {

using namespace aether::ui;

const char* DesktopSettingScreen::kWallNames[kWallCount] = { "dolphin" };

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
int DesktopSettingScreen::findWallIdx() const {
    std::string cur = rt_.config().getString("desktop", "wallpaper", kWallNames[0]);
    for (int i = 0; i < kWallCount; i++) if (cur == kWallNames[i]) return i;
    return 0;
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
    wallIdx_ = (wallIdx_ + dir + kWallCount) % kWallCount;
    rt_.config().setString("desktop", "wallpaper", kWallNames[wallIdx_]);
    rt_.view().requestRedraw();
}

void DesktopSettingScreen::fitAdj(void* u, int dir)    { static_cast<DesktopSettingScreen*>(u)->cycleFit(dir); }
void DesktopSettingScreen::anchorAdj(void* u, int dir) { static_cast<DesktopSettingScreen*>(u)->cycleAnchor(dir); }
void DesktopSettingScreen::wallAdj(void* u, int dir)   { static_cast<DesktopSettingScreen*>(u)->cycleWall(dir); }

void DesktopSettingScreen::onResume() {
    fitIdx_    = findFitIdx();
    anchorIdx_ = findAnchorIdx();
    wallIdx_   = findWallIdx();
    scroll_.scrollMain = 0;
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
        TitleBar(a, "Desktop"),
        ListContainer(a, scroll_, {
            ListSection(a, "Wallpaper"),
            input("Wallpaper", kWallNames[wallIdx_],            wallAdj),
            input("Fit",       shell::fitNames()[fitIdx_],      fitAdj),
            input("Anchor",    shell::anchorNames()[anchorIdx_], anchorAdj),
        }),
    });
}

} // namespace nema
