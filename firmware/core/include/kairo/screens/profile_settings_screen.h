#pragma once
#include "kairo/ui/component_screen.h"
#include "kairo/ui/virtual_keyboard.h"
#include <vector>

namespace kairo {

// ProfileSettingsScreen — lets the owner change their user name, device name,
// and password from the device itself (Settings → Profile). Uses TextInput for
// in-place editing (same overlay as the WiFi password entry). No auth guard:
// physical access to the device = you can change the profile.
class ProfileSettingsScreen : public ComponentScreen {
public:
    explicit ProfileSettingsScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;
    void        onAction(input::Action a) override;
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
    static void onSelect(void* u);

    ui::VirtualKeyboard kbd_;
    ui::ScrollState     scroll_;
    std::vector<Item> items_;
    Field             editField_ = Field::UserName;
    bool              editing_   = false;
    char              prompt_[48] = {};
};

} // namespace kairo
