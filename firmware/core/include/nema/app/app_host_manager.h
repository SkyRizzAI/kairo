#pragma once
#include <memory>

namespace nema {

class Runtime;
struct IApp;
class AppHost;
class CloseAndOpenModal;

// AppHostManager — single authority for launching apps and the Plan 22
// pause/resume + single-slot policy. Owns the foreground AppHost and (at most
// one) paused AppHost. All app launches route through launch() so the policy is
// enforced in one place. System screens (Settings etc.) are NOT apps and bypass
// this entirely.
class AppHostManager {
public:
    explicit AppHostManager(Runtime& rt);
    ~AppHostManager();

    // Launch an app. If another app is paused, shows the "Close & Open?" modal
    // instead (single-slot policy); otherwise launches immediately.
    void launch(IApp& app);

    // Pause the foreground app → park its thread, pop it, keep it alive.
    void pauseForeground();
    // Resume the paused app → re-push it (enter() clears the pause flag).
    void resumePaused();
    // Kill the paused app (exit + join + destroy).
    void killPaused();

    bool        hasPaused() const { return paused_ != nullptr; }
    const char* pausedName() const;

    // Unconditional launch — used by the modal after killing the paused app.
    void doLaunch(IApp& app);

private:
    Runtime&                           rt_;
    std::unique_ptr<AppHost>           foreground_;
    std::unique_ptr<AppHost>           paused_;
    std::unique_ptr<CloseAndOpenModal> modal_;
};

} // namespace nema
