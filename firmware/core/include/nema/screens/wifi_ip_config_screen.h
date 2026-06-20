#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/virtual_keyboard.h"
#include "nema/hal/wifi.h"
#include <string>

namespace nema {

class Runtime;

// Settings -> Wi-Fi -> <network> -> Configure IP (Plan 73).
//   < Automatic | Manual >
//   (Manual) IP Address / Subnet Mask / Router / DNS  -> tap a field to edit
//   [ Apply ]
class WifiIpConfigScreen : public ComponentScreen {
public:
    enum Field { F_IP = 0, F_MASK, F_ROUTER, F_DNS, F_COUNT };

    explicit WifiIpConfigScreen(Runtime& rt);

    void        onResume() override;
    void        onAction(input::Action a) override;
    void        onCode(input::Code c) override;
    void        draw(Canvas& c) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    enum class St { Form, EditField };

    IWifiDriver* drv();
    void redraw();
    void apply();
    void editField(Field f);

    static void cbToggleMode(void* u);
    static void cbEdit(void* u);
    static void cbApply(void* u);

    IWifiDriver*        drv_  = nullptr;
    St                  st_   = St::Form;
    bool                manual_ = false;
    WifiIpConfig        cfg_;                 // working copy
    Field               editing_ = F_IP;
    bool                swallowCode_ = false;
    aether::ui::VirtualKeyboard kbd_;
    char                prompt_[40] = {};
    aether::ui::ScrollState     scroll_;
    // Stable backing for field rows (label + value), 0..F_COUNT-1 plus mode/apply.
    struct FieldRow { WifiIpConfigScreen* self; Field f; std::string text; };
    FieldRow            fieldRows_[F_COUNT];
};

} // namespace nema
