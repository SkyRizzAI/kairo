// Plan 90 F6.15 — DeveloperScreen: action list + confirmation Dialog for destructive ops.
#include "nema/screens/developer_screen.h"
#include "nema/screens/confirm_modal.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/widgets.h"
#include "nema/input/input_action.h"

namespace nema {

using namespace aether::ui;

// Confirm callbacks — each runs the op then pops the modal (goBack).
static void doStopAether(void* u) {
    auto* rt = static_cast<Runtime*>(u);
    rt->switchDisplayServer("fbcon");
    rt->view().goBack();
}

static void doReboot(void* u) {
    static_cast<Runtime*>(u)->requestBootloader();
}

// ── DeveloperScreen ───────────────────────────────────────────────────────────

DeveloperScreen::DeveloperScreen(Runtime& rt) : ComponentScreen(rt) {
    auto* stop = new ConfirmModal(rt);
    stop->setup("Stop Aether?", "Switch to FbCon\ndisplay server.", "Stop", doStopAether, &rt);
    stopModal_.reset(stop);

    auto* reboot = new ConfirmModal(rt);
    reboot->setup("Reboot Device?", "Reboot to USB\nbootloader mode.", "Reboot", doReboot, &rt, /*danger=*/true);
    rebootModal_.reset(reboot);
}

void DeveloperScreen::onResume() {
    rt_.view().requestRedraw();
}

#define S(u) static_cast<DeveloperScreen*>(u)
UiNode* DeveloperScreen::build(NodeArena& a, Runtime&) {
    MenuBuilder m(a, scroll_, this);
    m.nav("Stop Aether Server",   [](void* u){ S(u)->rt_.view().push(*S(u)->stopModal_);   });
    m.nav("Reboot to Bootloader", [](void* u){ S(u)->rt_.view().push(*S(u)->rebootModal_); });
    return m.build();
}
#undef S

} // namespace nema
