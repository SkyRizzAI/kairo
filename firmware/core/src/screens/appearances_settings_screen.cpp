// Appearances settings: theme, desktop, launcher, asset pack, font.
#include "nema/screens/appearances_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/aether_server.h"
#include "nema/ui/canvas.h"
#include "nema/ui/font_registry.h"
#include "nema/config/config_store.h"
#include <cstring>
#include <string>

namespace nema {

using namespace aether::ui;

const char* AppearancesSettingsScreen::kThemeNames[kThemeCount] =
    {"default", "flipper"};   // colour themes (same fonts; palette differs)
const char* AppearancesSettingsScreen::kDesktopNames[kDesktopCount]   = {"livewal"};
const char* AppearancesSettingsScreen::kDesktopLabels[kDesktopCount]  = {"live wallpaper"};
const char* AppearancesSettingsScreen::kLauncherNames[kLauncherCount] = {"playsta", "wii", "compact", "flipper"};
const char* AppearancesSettingsScreen::kLauncherLabels[kLauncherCount]= {"Playstation 5", "Nintendo WII", "Compact", "Flipper"};
const char* AppearancesSettingsScreen::kAssetNames[kAssetCount]       = {"palanu"};

AppearancesSettingsScreen::AppearancesSettingsScreen(Runtime& rt)
    : ComponentScreen(rt), desktopSetting_(rt) {}

int AppearancesSettingsScreen::findThemeIdx() const {
    std::string cur = rt_.config().getString("aether", "theme", kThemeNames[0]);
    for (int i = 0; i < kThemeCount; i++)
        if (cur == kThemeNames[i]) return i;
    return 0;
}
int AppearancesSettingsScreen::findNameIdx(const char* ns, const char* key,
                                            const char* const* names, int count,
                                            const char* def) const {
    std::string cur = rt_.config().getString(ns, key, def);
    for (int i = 0; i < count; i++)
        if (cur == names[i]) return i;
    return 0;
}

void AppearancesSettingsScreen::applyTheme(int idx) {
    const char* name = kThemeNames[idx];
    // Theme = colour theme (same fonts/sizes; only the palette differs). default =
    // mono (white/black), flipper = orange/black. AetherServer pushes it next frame.
    aether::setColorTheme(std::strcmp(name, "flipper") == 0
                              ? aether::flipperColors() : aether::monoColors());
    rt_.config().setString("aether", "theme", name);
    rt_.view().requestRedraw();
}
void AppearancesSettingsScreen::cycleTheme(int dir) {
    themeIdx_ = (themeIdx_ + dir + kThemeCount) % kThemeCount;
    applyTheme(themeIdx_);
}
void AppearancesSettingsScreen::toggleDark() {
    darkOn_ = !darkOn_;
    aether::setDarkMode(darkOn_);
    rt_.config().setInt("aether", "dark", darkOn_ ? 1 : 0);
    rt_.view().requestRedraw();
}
void AppearancesSettingsScreen::cycleDesktop(int dir) {
    desktopIdx_ = (desktopIdx_ + dir + kDesktopCount) % kDesktopCount;
    rt_.config().setString("aether", "desktop", kDesktopNames[desktopIdx_]);
}
void AppearancesSettingsScreen::cycleLauncher(int dir) {
    launcherIdx_ = (launcherIdx_ + dir + kLauncherCount) % kLauncherCount;
    rt_.config().setString("aether", "launcher", kLauncherNames[launcherIdx_]);
}
void AppearancesSettingsScreen::cycleAsset(int dir) {
    assetIdx_ = (assetIdx_ + dir + kAssetCount) % kAssetCount;
    rt_.config().setString("aether", "assets", kAssetNames[assetIdx_]);
}

void AppearancesSettingsScreen::scanFontPacks() {
    fontPackCount_ = 0;
    // Index 0 is always the compiled-in Helvetica (no external file needed)
    std::strncpy(fontPackNames_[0], "Helvetica", 47);
    fontPackNames_[0][47] = '\0';
    fontPackPaths_[0][0]  = '\0';
    fontPackCount_ = 1;

    if (rt_.fs()) {
        char names[kMaxFontPacks - 1][48];
        char paths[kMaxFontPacks - 1][96];
        int n = nema::display::FontRegistry::instance().scanFontPacks(
            rt_.fs(), "/system/assets/fonts/",
            names, paths, kMaxFontPacks - 1);
        for (int i = 0; i < n && fontPackCount_ < kMaxFontPacks; i++) {
            std::strncpy(fontPackNames_[fontPackCount_], names[i], 47);
            fontPackNames_[fontPackCount_][47] = '\0';
            std::strncpy(fontPackPaths_[fontPackCount_], paths[i], 95);
            fontPackPaths_[fontPackCount_][95] = '\0';
            fontPackCount_++;
        }
    }

    // Determine current index from saved config
    fontPackIdx_ = 0;
    for (int i = 0; i < fontPackCount_; i++) {
        if (std::strcmp(fontPackNames_[i], fontName_) == 0) {
            fontPackIdx_ = i;
            break;
        }
    }
}

void AppearancesSettingsScreen::applyFont(const char* name) {
    std::strncpy(fontName_, name, 47);
    fontName_[47] = '\0';
    rt_.config().setString("aether", "font", name);

    auto& fontReg = nema::display::FontRegistry::instance();
    if (std::strcmp(name, "Helvetica") == 0 || name[0] == '\0') {
        // Restore compiled-in Helvetica (default) fonts
        fontReg.registerFont(nema::display::Fonts::Reg8,      &nema::display::FONT_REG8,   "reg8");
        fontReg.registerFont(nema::display::Fonts::Bold8,     &nema::display::FONT_BOLD8,  "bold8");
        fontReg.registerFont(nema::display::Fonts::Reg10,     &nema::display::FONT_REG10,  "reg10");
        fontReg.registerFont(nema::display::Fonts::Bold10,    &nema::display::FONT_BOLD10, "bold10");
        fontReg.registerFont(nema::display::Fonts::Reg12,     &nema::display::FONT_REG12,  "reg12");
        fontReg.registerFont(nema::display::Fonts::Bold12,    &nema::display::FONT_BOLD12, "bold12");
        fontReg.registerFont(nema::display::Fonts::Primary,   &nema::display::FONT_BOLD10, "primary");
        fontReg.registerFont(nema::display::Fonts::Secondary, &nema::display::FONT_REG8,   "secondary");
        fontReg.registerFont(nema::display::Fonts::Tiny,      &nema::display::FONT_REG8,   "tiny");
        fontReg.registerFont(nema::display::Fonts::BigNum,    &nema::display::FONT_BOLD12, "bignum");
    } else {
        // Find the pack path matching this name
        for (int i = 0; i < fontPackCount_; i++) {
            if (std::strcmp(fontPackNames_[i], name) == 0) {
                fontReg.applyFontPack(rt_.fs(), fontPackPaths_[i]);
                break;
            }
        }
    }
    rt_.view().requestRedraw();
}

void AppearancesSettingsScreen::cycleFont(int dir) {
    if (fontPackCount_ <= 1) return;
    fontPackIdx_ = (fontPackIdx_ + dir + fontPackCount_) % fontPackCount_;
    applyFont(fontPackNames_[fontPackIdx_]);
}

void AppearancesSettingsScreen::openDesktopSetting() {
    rt_.view().navigate(desktopSetting_);
}

void AppearancesSettingsScreen::themeAdj(void* u, int d)    { static_cast<AppearancesSettingsScreen*>(u)->cycleTheme(d); }
void AppearancesSettingsScreen::darkAdj(void* u, int)      { static_cast<AppearancesSettingsScreen*>(u)->toggleDark(); }
void AppearancesSettingsScreen::desktopAdj(void* u, int d)  { static_cast<AppearancesSettingsScreen*>(u)->cycleDesktop(d); }
void AppearancesSettingsScreen::launcherAdj(void* u, int d) { static_cast<AppearancesSettingsScreen*>(u)->cycleLauncher(d); }
void AppearancesSettingsScreen::assetAdj(void* u, int d)    { static_cast<AppearancesSettingsScreen*>(u)->cycleAsset(d); }
void AppearancesSettingsScreen::fontAdj(void* u, int d)     { static_cast<AppearancesSettingsScreen*>(u)->cycleFont(d); }
void AppearancesSettingsScreen::onDesktopSetting(void* u)   { static_cast<AppearancesSettingsScreen*>(u)->openDesktopSetting(); }

void AppearancesSettingsScreen::onResume() {
    themeIdx_    = findThemeIdx();
    darkOn_      = rt_.config().getIntOr("aether", "dark", 0) != 0;
    desktopIdx_  = findNameIdx("aether", "desktop",  kDesktopNames,  kDesktopCount,  kDesktopNames[0]);
    launcherIdx_ = findNameIdx("aether", "launcher", kLauncherNames, kLauncherCount, kLauncherNames[0]);
    assetIdx_    = findNameIdx("aether", "assets",   kAssetNames,    kAssetCount,    kAssetNames[0]);

    // Read saved font name
    std::string savedFont = rt_.config().getString("aether", "font", "Helvetica");
    std::strncpy(fontName_, savedFont.c_str(), 47);
    fontName_[47] = '\0';

    scanFontPacks();

    scroll_.scrollMain   = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

UiNode* AppearancesSettingsScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    auto input = [&](const char* label, const char* value, void (*adj)(void*, int)) {
        ListInput e; e.label = label; e.value = value; e.onAdjust = adj; e.user = this;
        return ListInputRow(a, e);
    };
    auto nav = [&](const char* label, void (*press)(void*)) {
        ListEntry e; e.label = label; e.chevron = true; e.onPress = press; e.user = this;
        return ListItemRow(a, e);
    };

    // The colour-theme selector only makes sense on a colour-capable panel; a true
    // B&W display (e-ink) hides it. Dark mode (= invert/swap) stays on both.
    bool color = rt_.canvas().driver().supportsColor();

    return View(a, root, {
        ListContainer(a, scroll_, {
            color ? input("Theme", kThemeNames[themeIdx_], themeAdj) : nullptr,
            input("Dark Mode",      darkOn_ ? "On" : "Off",        darkAdj),
            input("Desktop",        kDesktopLabels[desktopIdx_],   desktopAdj),
            nav  ("Desktop Setting",                               onDesktopSetting),
            input("Launcher",       kLauncherLabels[launcherIdx_], launcherAdj),
            input("Asset Pack",     kAssetNames[assetIdx_],        assetAdj),
            input("Font",           fontName_,                     fontAdj),
        }),
    });
}

} // namespace nema
