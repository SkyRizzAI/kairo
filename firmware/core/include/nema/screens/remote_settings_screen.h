#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/virtual_keyboard.h"
#include <string>

namespace nema {

class Runtime;
class RemoteAuthStore;

// Settings -> Remote (Plan 74). Controls the PLP remote layer (USB / BLE / Wi-Fi):
//   [Toggle] Remote Enabled
//   Password   Set / Change / Clear     (locks privileged channels: CLI/File/OTA)
//   Authorized devices: N -> Log Out All
class RemoteSettingsScreen : public ComponentScreen {
public:
    explicit RemoteSettingsScreen(Runtime& rt);

    void        onResume() override;
    void        onAction(input::Action a) override;
    void        onCode(input::Code c) override;
    void        draw(Canvas& c) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    enum class St { List, EnterPass };

    RemoteAuthStore* store();
    void redraw();

    RemoteAuthStore*    store_ = nullptr;
    St                  st_    = St::List;
    bool                swallowCode_ = false;
    aether::ui::VirtualKeyboard kbd_;
    aether::ui::ScrollState     scroll_;
    char                statusBuf_[48] = {};
};

} // namespace nema
