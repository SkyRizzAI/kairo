#pragma once
#include "kairo/ui/component_screen.h"
#include "kairo/screens/about_screen.h"
#include "kairo/screens/sleep_settings_screen.h"
#include "kairo/screens/controls_screen.h"
#include "kairo/apps/wifi_app.h"
#include "kairo/screens/touch_settings_screen.h"
#include "kairo/screens/sounds_settings_screen.h"
#include "kairo/screens/camera_settings_screen.h"
#include <memory>
#include <vector>

namespace kairo {

class Runtime;
class AppHost;

// Settings — component-migrated (Plan 30). Capability-gated, scrollable, tappable
// menu. Each row launches a sub-screen or app (WiFi / Scroll Demo run as apps).
class SettingsScreen : public ComponentScreen {
public:
    explicit SettingsScreen(Runtime& rt);
    ~SettingsScreen() override;   // out-of-line: AppHost incomplete here
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    enum Kind { WiFi, Display, Controls, Touch, Sounds, Camera, About };
    struct Item { SettingsScreen* self; Kind kind; const char* label; };

    AboutScreen          about_;
    SleepSettingsScreen  sleepSettings_;
    ControlsScreen       controls_;
    WifiApp              wifiApp_;
    TouchSettingsScreen  touchSettings_;
    SoundsSettingsScreen sounds_;
    CameraSettingsScreen cameraSettings_;
    std::unique_ptr<AppHost> appHost_;   // hosts WiFi / Scroll Demo

    ui::ScrollState   scroll_;
    std::vector<Item> items_;

    static void onSelect(void* u);
    void        launch(Kind k);
};

} // namespace kairo
