#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/sounds_settings_screen.h"
#include "nema/screens/camera_settings_screen.h"
#include "nema/screens/touch_settings_screen.h"
#include "nema/screens/led_settings_screen.h"
#include "nema/screens/sensors_settings_screen.h"
#include "nema/screens/battery_settings_screen.h"
#include "nema/screens/secure_settings_screen.h"
#include <vector>

namespace nema {

class Runtime;

// Settings → Device & Hardware. Groups the per-peripheral screens (Sounds,
// Camera, Touch, LEDs, Sensors, Battery, Secure Element) under one entry, each
// capability-gated so only present hardware shows. Owns the sub-screens.
class DeviceHardwareScreen : public ComponentScreen {
public:
    explicit DeviceHardwareScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    enum Kind { Sounds, Camera, Touch, Led, Sensors, Battery, Secure };
    struct Item { DeviceHardwareScreen* self; Kind kind; const char* label; };

    SoundsSettingsScreen  sounds_;
    CameraSettingsScreen  cameraSettings_;
    TouchSettingsScreen   touchSettings_;
    LedSettingsScreen     ledSettings_;
    SensorsSettingsScreen sensorsSettings_;
    BatterySettingsScreen batterySettings_;
    SecureSettingsScreen  secureSettings_;

    aether::ui::ScrollState scroll_;
    std::vector<Item>       items_;

    static void onSelect(void* u);
    void        launch(Kind k);
};

} // namespace nema
