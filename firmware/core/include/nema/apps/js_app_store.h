#pragma once
#include "nema/apps/js_app.h"
#include "nema/app/app_manifest.h"
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace nema {

class Runtime;

// JsAppStore — the custom-app store. A process-wide owner of JS apps (.papp
// bundles): the built-in (embedded) ones AND any installed at runtime over the
// wire (OTA via PLP). Each installed app is registered into the AppRegistry as
// AppKind::Custom, so it appears in the launcher next to built-ins.
//
// Installing is FILESYSTEM-FREE: an app pushed over the wire is registered live
// in RAM and appears immediately (volatile — lost on reboot). Persistent install
// (survives reboot) only needs a flash filesystem (SPIFFS/LittleFS on internal
// flash) — a microSD is NOT required; SD is only for bulk/removable libraries
// (Plan 37). This is Palanu's equivalent of Flipper's FAP loader, but the package
// is portable JS rather than an arch-specific binary.
class JsAppStore {
public:
    static JsAppStore& instance();

    // Build + install a JS app live (appears in the launcher now). Refuses a
    // duplicate id (the running instance must not be swapped out from under it).
    // Returns false on bad input or duplicate.
    // displayServer: preferred server from the manifest (Plan 51); empty = any.
    bool installApp(Runtime& rt, std::string id, std::string name,
                    std::string version, std::string js,
                    std::string displayServer = "",
                    AppMode mode = AppMode::Ui,
                    std::vector<uint8_t> iconData = {});

    // Install from a single-file PAPP1 container (Plan 59):
    //   PAPP1\n<manifest-json>\n<entry-filename>\n<js>
    // Parses manifest fields (id, name, version, display_server, api_version).
    // The OTA entry point; also used by the WASM platform for live installs.
    bool installPappBytes(Runtime& rt, const char* bytes, size_t len);

    int count() const { return (int)apps_.size(); }

private:
    std::list<std::unique_ptr<JsApp>> apps_;   // list: stable pointers (vector realloc would invalidate)
};

// Convenience: install all built-in JS apps (the embedded store, Plan 37 Fase 5).
void loadEmbeddedJsApps(Runtime& rt);

} // namespace nema
