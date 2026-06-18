#include "nema/screens/close_and_open_modal.h"
#include "nema/app/app_host_manager.h"
#include "nema/app/app.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/view_dispatcher.h"
#include <cstdio>

namespace nema {

CloseAndOpenModal::CloseAndOpenModal(Runtime& rt, AppHostManager& mgr, IApp& target)
    : ComponentScreen(rt), mgr_(mgr), target_(target) {
    std::snprintf(title_, sizeof(title_), "%s is running. Close to open %s?",
                  mgr_.pausedName() ? mgr_.pausedName() : "App",
                  target_.name());
}

void CloseAndOpenModal::onResume() {
    ComponentScreen::onResume();
}

void CloseAndOpenModal::onCloseAndOpen(void* ctx) {
    auto* self = static_cast<CloseAndOpenModal*>(ctx);
    self->rt_.view().goBack();       // dismiss modal
    self->mgr_.killPaused();
    self->mgr_.doLaunch(self->target_);
}

void CloseAndOpenModal::onCancel(void* ctx) {
    auto* self = static_cast<CloseAndOpenModal*>(ctx);
    self->rt_.view().goBack();
}

ui::UiNode* CloseAndOpenModal::build(ui::NodeArena& a, Runtime& /*rt*/) {
    ui::DialogButton buttons[2] = {
        {"Close & Open", onCloseAndOpen, this},
        {"Cancel",       onCancel,       this},
    };
    return ui::Dialog(a, nullptr, title_, nullptr, 0, 0, buttons, 2);
}

} // namespace nema
