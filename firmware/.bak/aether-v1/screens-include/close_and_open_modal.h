#pragma once
#include "nema/ui/screen.h"

namespace nema {

class Runtime;
struct IApp;
class AppHostManager;

// Confirm dialog shown when launching an app while another is paused (Plan 22
// single-slot policy). "Close & Open" kills the paused app and launches the new
// one; "Cancel" keeps the paused app. Small immediate-mode Modal screen.
class CloseAndOpenModal : public IScreen {
public:
    CloseAndOpenModal(Runtime& rt, AppHostManager& mgr, IApp& target);

    ScreenMode mode()        const override { return ScreenMode::Modal; }
    uint16_t   modalWidth()  const override { return 220; }
    uint16_t   modalHeight() const override { return 86; }

    void enter() override;
    void onAction(input::Action a) override;
    void draw(Canvas& c) override;

private:
    Runtime&        rt_;
    AppHostManager& mgr_;
    IApp&           target_;
    int             cursor_ = 0;   // 0 = Close & Open, 1 = Cancel
};

} // namespace nema
