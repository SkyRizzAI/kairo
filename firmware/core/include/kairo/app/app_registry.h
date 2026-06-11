#pragma once
#include "kairo/app/app_manifest.h"
#include <vector>

namespace kairo {

class Runtime;
struct IApp;
struct IScreen;
struct IService;

// AppRegistry — the installed-app table. This is the device's app store index:
// every launchable app lives here, whether it was compiled into the firmware
// (install()) or installed at runtime (installCustom() — a .kapp pushed over KLP
// or loaded from storage). The launcher reads list(); selecting an entry calls
// launch(id), which spawns the app on its own thread via the AppHostManager.
//
// (Flipper analogy: built-ins are the fbt-generated .fam registry; custom apps
// are FAPs. Kairo's custom apps are JS .kapp bundles — portable across boards,
// unlike arch-specific native binaries.)
//
// Background daemons are manifest entries too (AppType::Service — Flipper's
// apptype=SERVICE): hidden from the launcher, lifecycle owned by the Nema
// ServiceManager. System screens (Settings etc.) and Settings' sub-apps
// (Wifi/Bluetooth) are launched directly via AppHostManager / ViewDispatcher
// and are intentionally absent from the registry.
class AppRegistry {
public:
    explicit AppRegistry(Runtime& rt);

    // Install a built-in app (compiled into the firmware). Re-installing an id
    // replaces the existing entry (so a rebuilt instance can swap in).
    void install(IApp& app, const char* version = "1.0.0");

    // Install a custom app (installed at runtime — e.g. a JS .kapp). Same as
    // install() but tagged AppKind::Custom so the UI can distinguish origin.
    void installCustom(IApp& app, const char* version = "1.0.0");

    // Install a screen-backed app: one whose UI is a system IScreen pushed on the
    // view stack, rather than an IApp run on its own thread. For apps that are
    // inherently UI-thread (e.g. the live camera viewfinder). launch() pushes the
    // screen. Identity is explicit since IScreen carries none.
    void installScreen(const char* id, const char* name, IScreen& screen,
                       const char* version = "1.0.0");

    // Install a background service (AppType::Service — Flipper's apptype=SERVICE).
    // Hidden from the launcher; lifecycle goes to the Nema ServiceManager.
    // Installed before rt.start(): boots with the system. Installed while
    // Running (e.g. by an app at runtime): starts immediately and ticks from the
    // next frame. Identity is explicit since IService only carries a name.
    void installService(IService& svc, const char* id, const char* version = "1.0.0");

    // Remove an app by id. No effect if absent.
    void uninstall(const char* id);

    bool isInstalled(const char* id) const;

    // Flat list of installed apps for the launcher (insertion order).
    const std::vector<AppManifest>& list() const { return manifests_; }

    // Launch an installed app on its own thread. Returns false if id is unknown.
    bool launch(const char* id);

private:
    // What a manifest entry points at: a threaded app (AppHost), a UI-thread
    // screen (view stack), or a background service (ServiceManager). Exactly one
    // is set.
    struct Target {
        IApp*     app     = nullptr;
        IScreen*  screen  = nullptr;
        IService* service = nullptr;
    };
    void add(const AppManifest& m, Target t);

    Runtime&                 rt_;
    std::vector<Target>      targets_;     // index-aligned with manifests_
    std::vector<AppManifest> manifests_;
};

} // namespace kairo
