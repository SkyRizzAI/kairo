// Plan 90 F6.15 — DeveloperScreen: action list + confirmation Dialog for destructive ops.
#include "nema/screens/developer_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/widgets.h"
#include "nema/input/input_action.h"

namespace nema {

using namespace aether::ui;

// Forward-declare confirm callbacks so ConfirmModal can reference them.
static void doCancel(void* u);
static void doStopAether(void* u);
static void doReboot(void* u);

// ── ConfirmModal: generic "Title / Body / Cancel + Action" modal ─────────────
class ConfirmModal : public ComponentScreen {
public:
    ConfirmModal(Runtime& rt, const char* title, const char* body,
                 const char* actionLabel, void (*onConfirm)(void*), void* userdata,
                 bool danger = false)
        : ComponentScreen(rt, 16)
        , title_(title), body_(body)
        , actionBtn_{actionLabel, onConfirm, userdata, danger}
        , cancelBtn_{"Cancel", doCancel, &rt, false}
    {}

    ScreenMode mode()        const override { return ScreenMode::Modal; }
    uint16_t   modalWidth()  const override { return 210; }
    uint16_t   modalHeight() const override { return 80;  }

    void onAction(input::Action a) override {
        if (a == input::Action::Back) rt_.view().goBack();
    }

    UiNode* build(NodeArena& a, Runtime&) override {
        DialogButton btns[2] = {cancelBtn_, actionBtn_};
        // Cancel left, action right (right = "confirm" position by convention).
        return Dialog(a, title_, body_, nullptr, 0, 0, btns, 2);
    }

private:
    const char*  title_;
    const char*  body_;
    DialogButton actionBtn_;
    DialogButton cancelBtn_;
};

// ── Confirm callbacks ─────────────────────────────────────────────────────────

static void doCancel(void* u) {
    static_cast<Runtime*>(u)->view().goBack();
}

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
    stopModal_ = std::unique_ptr<ComponentScreen>(
        new ConfirmModal(rt, "Stop Aether?",
                         "Switch to FbCon\ndisplay server.",
                         "Stop", doStopAether, &rt));
    rebootModal_ = std::unique_ptr<ComponentScreen>(
        new ConfirmModal(rt, "Reboot Device?",
                         "Reboot to USB\nbootloader mode.",
                         "Reboot", doReboot, &rt, /*danger=*/true));
}

void DeveloperScreen::onResume() {
    scroll_.scrollMain   = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

void DeveloperScreen::onStopAetherPressed(void* u) {
    auto* self = static_cast<DeveloperScreen*>(u);
    self->rt_.view().push(*self->stopModal_);
}

void DeveloperScreen::onRebootPressed(void* u) {
    auto* self = static_cast<DeveloperScreen*>(u);
    self->rt_.view().push(*self->rebootModal_);
}

UiNode* DeveloperScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    ListEntry stopEntry;
    stopEntry.label   = "Stop Aether Server";
    stopEntry.chevron = true;
    stopEntry.onPress = onStopAetherPressed;
    stopEntry.user    = this;

    ListEntry rebootEntry;
    rebootEntry.label   = "Reboot to Bootloader";
    rebootEntry.chevron = true;
    rebootEntry.onPress = onRebootPressed;
    rebootEntry.user    = this;

    return View(a, root, {
        ListContainer(a, scroll_, {
            ListItemRow(a, stopEntry),
            ListItemRow(a, rebootEntry),
        }),
    });
}

} // namespace nema
