#pragma once
#include "nema/ui/screen.h"
#include "nema/ui/component_screen.h"

namespace nema {

class Runtime;
struct IApp;
class AppHostManager;

// Plan 70 rewrite: confirmation dialog using the Dialog widget builder.
// Shown when launching an app while another is paused (Plan 22 single-slot
// policy). "Close & Open" kills the paused app and launches the new one;
// "Cancel" keeps the paused app.
class CloseAndOpenModal : public ComponentScreen {
public:
    CloseAndOpenModal(Runtime& rt, AppHostManager& mgr, IApp& target);

    ScreenMode mode()        const override { return ScreenMode::Modal; }
    uint16_t   modalWidth()  const override { return 220; }
    uint16_t   modalHeight() const override { return 86; }

    void onResume() override;

    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    AppHostManager& mgr_;
    IApp&           target_;
    char            title_[48] = "";

    static void onCloseAndOpen(void* ctx);
    static void onCancel(void* ctx);
};

} // namespace nema
