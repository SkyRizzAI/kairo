#include "nema/screens/developer_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"

namespace nema {

using namespace ui;

DeveloperScreen::DeveloperScreen(Runtime& rt) : ComponentScreen(rt) {}

void DeveloperScreen::onResume() {
    scroll_.scrollMain = 0;
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
    uint8_t pad = nema::theme().space.sm;
    uint8_t gap = nema::theme().space.xs;
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = pad; root.gap = gap;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = gap;

    return View(a, root, {
        TitleBar(a, "DEVELOPER"),
        ScrollView(a, scroll_, sv, {
            ListItem(a, "Reboot to Bootloader", ">", onRebootBootloader, this),
            ListItem(a, "Stop Aether Server", ">", onStopAether, this),
        }),
    });
}

} // namespace nema
