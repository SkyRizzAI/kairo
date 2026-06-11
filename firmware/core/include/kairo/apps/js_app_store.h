#pragma once
#include "kairo/apps/js_app.h"
#include <memory>
#include <string>
#include <vector>

namespace kairo {

class Runtime;

// JsAppStore — the custom-app store. A process-wide owner of JS apps (.kapp
// bundles): the built-in (embedded) ones AND any installed at runtime over the
// wire (OTA via KLP). Each installed app is registered into the AppRegistry as
// AppKind::Custom, so it appears in the launcher next to built-ins.
//
// Installing is FILESYSTEM-FREE: an app pushed over the wire is registered live
// in RAM and appears immediately (volatile — lost on reboot). Persistent install
// (survives reboot) only needs a flash filesystem (SPIFFS/LittleFS on internal
// flash) — a microSD is NOT required; SD is only for bulk/removable libraries
// (Plan 37). This is Kairo's equivalent of Flipper's FAP loader, but the package
// is portable JS rather than an arch-specific binary.
class JsAppStore {
public:
    static JsAppStore& instance();

    // Build + install a JS app live (appears in the launcher now). Refuses a
    // duplicate id (the running instance must not be swapped out from under it).
    // Returns false on bad input or duplicate.
    bool installApp(Runtime& rt, std::string id, std::string name,
                    std::string version, std::string js);

    // Install from a .kapp container ("KAPP1\n<manifest-json>\n<js>"): parses the
    // id/name/version from the manifest, then installApp(). The OTA entry point.
    bool installKapp(Runtime& rt, const char* kappBytes, size_t len);

    int count() const { return (int)apps_.size(); }

private:
    std::vector<std::unique_ptr<JsApp>> apps_;   // owns every installed JS app
};

// Convenience: install all built-in JS apps (the embedded store, Plan 37 Fase 5).
void loadEmbeddedJsApps(Runtime& rt);

} // namespace kairo
