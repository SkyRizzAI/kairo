#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/about_screen.h"
#include "nema/screens/sleep_settings_screen.h"
#include "nema/screens/appearances_settings_screen.h"
#include "nema/screens/wifi_settings_screen.h"
#include "nema/screens/bluetooth_settings_screen.h"
#include "nema/screens/remote_settings_screen.h"
#include "nema/screens/controls_screen.h"
#include "nema/screens/touch_settings_screen.h"
#include "nema/screens/sounds_settings_screen.h"
#include "nema/screens/camera_settings_screen.h"
#include "nema/screens/led_settings_screen.h"
#include "nema/screens/sensors_settings_screen.h"
#include "nema/screens/developer_screen.h"
#include "nema/screens/profile_settings_screen.h"
#include "nema/screens/storage_settings_screen.h"
#include "nema/screens/app_list_screen.h"
#include "nema/screens/app_detail_screen.h"
#include <memory>
#include <vector>

namespace nema {

class Runtime;

// Settings — Plan 60 ListView. Capability-gated, scrollable menu.
// WiFi/Bluetooth entries removed (apps migrated to .bak — Plan 60 cleanup).
class SettingsScreen : public ComponentScreen {
public:
    explicit SettingsScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    enum Kind { Display, Appearances, Controls, Wifi, Bluetooth, Remote, Touch, Sounds, Camera, Led, Sensors, Developer, About, Profile, Storage, Apps };
    struct Item { SettingsScreen* self; Kind kind; const char* label; };

    AboutScreen          about_;
    SleepSettingsScreen        sleepSettings_;
    AppearancesSettingsScreen  appearances_;
    WifiSettingsScreen   wifiSettings_;
    BluetoothSettingsScreen btSettings_;
    RemoteSettingsScreen remoteSettings_;
    ControlsScreen       controls_;
    TouchSettingsScreen  touchSettings_;
    SoundsSettingsScreen sounds_;
    CameraSettingsScreen  cameraSettings_;
    LedSettingsScreen     ledSettings_;
    SensorsSettingsScreen sensorsSettings_;
    DeveloperScreen      developer_;
    ProfileSettingsScreen profileSettings_;
    StorageSettingsScreen storageSettings_;
    AppDetailScreen       appDetail_;
    AppListScreen         appsSettings_;   // settings-context list → opens appDetail_

    aether::ui::ScrollState   scroll_;
    std::vector<Item> items_;

    static void onSelect(void* u);
    void        launch(Kind k);
};

} // namespace nema
