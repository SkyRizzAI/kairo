#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/settings_screen.h"
#include "nema/screens/wifi_settings_screen.h"
#include "nema/screens/splash_screen.h"

namespace nema {

class Runtime;

// Mission Control — Flipper-style quick-settings panel (Plan 92 — Control Center).
// A responsive tile grid (dark mode, wifi, lock, settings, restart) + two vertical
// sliders (display brightness, speaker volume). Opened from the desktop via Up/Left;
// the grid reflows for portrait vs landscape (LCD-agnostic).
class MissionControlScreen : public ComponentScreen {
public:
    explicit MissionControlScreen(Runtime& rt);
    void                onResume() override;
    void                onAction(input::Action a) override;   // custom 2D grid nav
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    void activate(int f);            // fire the focused tile's action
    void adjustBar(int f, int dir);  // ±step a focused slider
    aether::ui::UiNode* tile(aether::ui::NodeArena& a, const uint8_t* icon,
                             uint8_t iw, uint8_t ih, void (*press)(void*));

    static void onDark(void*);
    static void onWifi(void*);
    static void onLock(void*);
    static void onSettings(void*);
    static void onRestart(void*);
    static void onBrightness(void* u, int v);
    static void onVolume(void* u, int v);

    SettingsScreen     settings_;      // opened by the Settings tile
    WifiSettingsScreen wifiSettings_;  // opened when WiFi-on has nothing to auto-join
    SplashScreen       restartSplash_; // shown for 2s before a restart (aether-side)
    int  brightness_ = 255;
    int  volume_     = 100;            // 0..100 %
    bool darkOn_     = false;
    bool wifiOn_     = false;
};

} // namespace nema
