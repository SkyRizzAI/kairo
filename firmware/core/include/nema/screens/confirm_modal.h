#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/widgets.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/input/input_action.h"
#include "nema/runtime.h"
#include <cstdio>

namespace nema {

// Shared confirmation modal: "Title / body / [Cancel] [Action]". Reusable + self-contained —
// it COPIES its strings, so dynamic text like "Forget <SSID>?" is safe. Own one as a screen
// member, setup() it just before showing, then rt_.view().push(confirm_). Cancel pops the
// modal; the Action button calls your onConfirm callback (which should pop the modal and run
// the destructive op, e.g. via the parent screen's runBusy()). Gate every destructive action
// (forget / clear / uninstall / eject / reset) through this so a stray Activate can't nuke data.
class ConfirmModal : public ComponentScreen {
public:
    explicit ConfirmModal(Runtime& rt) : ComponentScreen(rt, 16) {}

    void setup(const char* title, const char* body, const char* actionLabel,
               void (*onConfirm)(void*), void* user, bool danger = false) {
        std::snprintf(title_,       sizeof(title_),       "%s", title       ? title       : "");
        std::snprintf(body_,        sizeof(body_),        "%s", body        ? body        : "");
        std::snprintf(actionLabel_, sizeof(actionLabel_), "%s", actionLabel ? actionLabel : "OK");
        onConfirm_ = onConfirm;
        user_      = user;
        danger_    = danger;
    }

    ScreenMode mode()        const override { return ScreenMode::Modal; }
    uint16_t   modalWidth()  const override { return 210; }
    uint16_t   modalHeight() const override { return 80;  }

    void onAction(input::Action a) override {
        if (a == input::Action::Back) { rt_.view().goBack(); return; }
        ComponentScreen::onAction(a);   // Activate fires the focused DialogButton
    }

    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime&) override {
        aether::ui::DialogButton btns[2] = {
            { "Cancel",      &ConfirmModal::cancelThunk, this, false },
            { actionLabel_,  onConfirm_,                 user_, danger_ },
        };
        return aether::ui::Dialog(a, title_, body_, nullptr, 0, 0, btns, 2);
    }

private:
    static void cancelThunk(void* u) { static_cast<ConfirmModal*>(u)->rt_.view().goBack(); }

    char  title_[40]       = {};
    char  body_[80]        = {};
    char  actionLabel_[24] = {};
    void (*onConfirm_)(void*) = nullptr;
    void*  user_  = nullptr;
    bool   danger_ = false;
};

} // namespace nema
