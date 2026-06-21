#include "nema/screens/developer_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"

namespace nema {

using namespace aether::ui;

DeveloperScreen::DeveloperScreen(Runtime& rt) : ComponentScreen(rt) {}

void DeveloperScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

void DeveloperScreen::stopAether() {
    rt_.switchDisplayServer("fbcon");
}

void DeveloperScreen::rebootBootloader() {
    rt_.requestBootloader();
}

void DeveloperScreen::onStopAether(void* u) {
    static_cast<DeveloperScreen*>(u)->stopAether();
}

void DeveloperScreen::onRebootBootloader(void* u) {
    static_cast<DeveloperScreen*>(u)->rebootBootloader();
}

UiNode* DeveloperScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    ListEntry reboot; reboot.label = "Reboot to Bootloader"; reboot.chevron = true;
    reboot.onPress = onRebootBootloader; reboot.user = this;

    ListEntry stopAether; stopAether.label = "Stop Aether Server"; stopAether.chevron = true;
    stopAether.onPress = onStopAether; stopAether.user = this;

    return View(a, root, {
        TitleBar(a, "Developer"),
        ListContainer(a, scroll_, {
            ListItemRow(a, reboot),
            ListItemRow(a, stopAether),
        }),
    });
}

} // namespace nema
