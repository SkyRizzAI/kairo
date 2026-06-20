#pragma once
#include "nema/ui/component_screen.h"
#include "nema/hal/bluetooth.h"
#include <vector>
#include <string>
#include <atomic>

namespace nema {

class Runtime;

// Settings → Bluetooth (Plan 73). ComponentScreen gated on caps::BtBle: enable
// toggle, discoverable (advertising) toggle, numeric-comparison pairing prompt,
// and the bonded-device list. The BLE adapter also backs the PLP remote cable;
// this screen only manages the radio/pairing/bonds.
class BluetoothSettingsScreen : public ComponentScreen {
public:
    explicit BluetoothSettingsScreen(Runtime& rt);

    void        onResume() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    // name is std::string because ListItem stores the char* pointer (not a copy);
    // a stack buffer would dangle before the frame renders.
    struct BondRow { BluetoothSettingsScreen* self; uint8_t addr[6]; std::string name; };

    IBluetoothController* ctrl();
    IBleAdapter*          ble();
    void setEnabled(bool on);
    void redraw();   // mark model dirty (rebuild tree) + request a view redraw

    static void cbToggleEnable(void* u);
    static void cbToggleAdv(void* u);
    static void cbConfirmPair(void* u);
    static void cbRejectPair(void* u);
    static void cbForget(void* u);

    IBluetoothController* ctrl_ = nullptr;
    IBleAdapter*          ble_  = nullptr;
    ui::ScrollState       scroll_;
    char                  statusBuf_[72] = {};
    bool                  pendingPair_ = false;
    std::string           passkey_;
    std::string           pairPrompt_;   // stable backing for the pairing Text node
    std::vector<BondRow>  bonds_;
    std::atomic<bool>     busy_{false};
};

} // namespace nema
