#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/virtual_keyboard.h"
#include "nema/screens/confirm_modal.h"
#include <vector>

namespace nema {

// ProfileSettingsScreen — lets the owner change their user name, device name,
// and password from the device itself (Settings → Profile). Uses TextInput for
// in-place editing (same overlay as the WiFi password entry). No auth guard:
// physical access to the device = you can change the profile.
class ProfileSettingsScreen : public ComponentScreen {
public:
    explicit ProfileSettingsScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;
    void        onAction(input::Action a) override;
    void        onCode(input::Code c) override;
    void        draw(Canvas& c) override;

private:
    enum class Field { UserName, DeviceName, SetPassword, ClearPassword };
    struct Item {
        ProfileSettingsScreen* self;
        Field                  field;
        const char*            label;
        char                   detail[32];
    };

    void        startEdit(Field f);
    void        applyKbdResult(bool done, bool cancel);
    static void onSelect(void* u);
    static void doClearPassword(void* u);   // runs after the user confirms

    aether::ui::VirtualKeyboard kbd_;
    aether::ui::ScrollState     scroll_;
    ConfirmModal      confirm_;
    std::vector<Item> items_;
    Field             editField_ = Field::UserName;
    bool              editing_     = false;
    bool              swallowCode_ = false;
    char              prompt_[48]  = {};
};

} // namespace nema
