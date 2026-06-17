#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/about_screen.h"
#include "nema/screens/sleep_settings_screen.h"
#include "nema/screens/controls_screen.h"
#include "nema/apps/wifi_app.h"
#include "nema/apps/bluetooth_app.h"
#include "nema/screens/touch_settings_screen.h"
#include "nema/screens/sounds_settings_screen.h"
#include "nema/screens/camera_settings_screen.h"
#include "nema/screens/profile_settings_screen.h"
#include <memory>
#include <vector>

namespace nema {

class Runtime;

// Settings — component-migrated (Plan 30). Capability-gated, scrollable, tappable
// menu. Apps (WiFi / Bluetooth) launch via AppHostManager (Plan 22 single-slot).
class SettingsScreen : public ComponentScreen {
public:
    explicit SettingsScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    enum Kind { WiFi, Bluetooth, Display, Controls, Touch, Sounds, Camera, About, Profile };
    struct Item { SettingsScreen* self; Kind kind; const char* label; };

    AboutScreen          about_;
    SleepSettingsScreen  sleepSettings_;
    ControlsScreen       controls_;
    WifiApp              wifiApp_;
    BluetoothApp         bluetoothApp_;
    TouchSettingsScreen  touchSettings_;
    SoundsSettingsScreen sounds_;
    CameraSettingsScreen  cameraSettings_;
    ProfileSettingsScreen profileSettings_;

    ui::ScrollState   scroll_;
    std::vector<Item> items_;

    static void onSelect(void* u);
    void        launch(Kind k);
};

} // namespace nema
