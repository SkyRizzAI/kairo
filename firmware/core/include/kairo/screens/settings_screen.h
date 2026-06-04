#pragma once
#include "kairo/ui/screen.h"
#include "kairo/screens/about_screen.h"
#include "kairo/screens/sleep_settings_screen.h"
#include "kairo/screens/controls_screen.h"
#include "kairo/apps/wifi_app.h"
#include <memory>
#include <vector>

namespace kairo {

class Runtime;
class AppHost;

class SettingsScreen : public IScreen {
public:
    explicit SettingsScreen(Runtime& rt);
    ~SettingsScreen() override;   // out-of-line: AppHost incomplete here
    void enter()         override;
    void update(Key key) override;
    void draw(Canvas& c) override;

private:
    Runtime&            rt_;
    AboutScreen         about_;
    SleepSettingsScreen sleepSettings_;
    ControlsScreen      controls_;
    int                 cursor_ = 0;

    WifiApp                  wifiApp_;
    std::unique_ptr<AppHost> wifiHost_;   // launched on WiFi select

    struct Item { const char* label; };
    std::vector<Item> items_;

    void buildMenu();
    void handleSelect();
};

} // namespace kairo
