#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <functional>

namespace nema {

class Runtime;
struct IApp;
class AppHost;
struct IScreen;
struct Event;

// AppHostManager — single authority for launching apps and the Plan 22
// pause/resume + single-slot policy. Owns the foreground AppHost and (at most
// one) paused AppHost. All app launches route through launch() so the policy is
// enforced in one place. System screens (Settings etc.) are NOT apps and bypass
// this entirely.
//
// Plan 64 — crash recovery: detects AppHostExited events, tracks crash count
// per app, and auto-restarts with backoff (0s → 5s → 30s → FAILED after 4x).
class AppHostManager {
public:
    explicit AppHostManager(Runtime& rt);
    ~AppHostManager();

    // Init crash-recovery subscriptions (called after EventBus is ready).
    void initCrashRecovery();

    // Launch an app with optional argv (Plan 86: [id] + manifest.args).
    // If another app is paused, shows the "Close & Open?" modal instead.
    void launch(IApp& app, std::vector<std::string> argv = {});

    // Pause the foreground app → park its thread, pop it, keep it alive.
    void pauseForeground();
    // Resume the paused app → re-push it (enter() clears the pause flag).
    void resumePaused();
    // Kill the paused app (exit + join + destroy).
    void killPaused();

    bool        hasPaused() const { return paused_ != nullptr; }
    const char* pausedName() const;
    bool        hasForeground() const { return foreground_ != nullptr; }
    const char* foregroundName() const;   // running app name, "" if none (Plan 46)

    // Unconditional launch — used by the modal after killing the paused app.
    void doLaunch(IApp& app, std::vector<std::string> argv = {});

    // Plan 80: the app-switch transition ("Close & Open?" modal) is a presentation
    // screen owned by the display server (aether), not the kernel. The display
    // server installs a factory at boot; with no factory (headless) the manager
    // falls back to killing the paused app and launching immediately.
    using TransitionModalFactory =
        std::function<std::unique_ptr<IScreen>(Runtime&, AppHostManager&, IApp&)>;
    void setTransitionModalFactory(TransitionModalFactory f) { modalFactory_ = std::move(f); }

    // Plan 64 — crash recovery event handlers.
    void onAppExited(const Event& e);
    void onClockTick(const Event& e);

    // Check if an app has been marked FAILED (4+ crashes in 60s).
    bool isFailed(const std::string& appId) const;

private:
    static constexpr uint32_t kMaxCrashes = 4;
    static constexpr uint64_t kCrashWindowMs = 60000;

    struct CrashRecord {
        uint32_t crashCount = 0;
        uint64_t firstCrashMs = 0;
    };

    struct PendingRestart {
        std::string appId;
        uint64_t restartAtMs = 0;
        uint32_t attempt = 0;
    };

    Runtime&                           rt_;
    std::unique_ptr<AppHost>           foreground_;
    std::unique_ptr<AppHost>           paused_;
    std::unique_ptr<IScreen>           modal_;       // the active transition modal
    TransitionModalFactory             modalFactory_;// installed by the display server
    std::unordered_map<std::string, CrashRecord> crashMap_;
    std::unique_ptr<PendingRestart>    pendingRestart_;
};

} // namespace nema
