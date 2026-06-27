#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/confirm_modal.h"
#include "nema/hal/bluetooth.h"
#include <vector>
#include <string>
#include <cstdint>

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
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    // name is std::string because ListItem stores the char* pointer (not a copy);
    // a stack buffer would dangle before the frame renders.
    struct BondRow { BluetoothSettingsScreen* self; uint8_t addr[6]; std::string name; };

    IBluetoothController* ctrl();
    IBleAdapter*          ble();
    void setEnabled(bool on);
    void redraw();   // mark model dirty (rebuild tree) + request a view redraw

    static void cbForget(void* u);   // shows the confirm modal
    static void doForget(void* u);   // runs after the user confirms

    IBluetoothController* ctrl_ = nullptr;
    IBleAdapter*          ble_  = nullptr;
    aether::ui::ScrollState       scroll_;
    char                  statusBuf_[72] = {};
    bool                  pendingPair_ = false;
    std::string           passkey_;
    std::string           pairPrompt_;   // stable backing for the pairing Text node
    std::vector<BondRow>  bonds_;
    ConfirmModal          confirm_;
    char                  confirmBody_[64] = {};
    uint8_t               pendingAddr_[6] = {};   // device addr to forget (copied; survives rebuild)
};

} // namespace nema
