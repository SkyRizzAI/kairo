#pragma once
#include "kairo/plugins/js_app_plugin.h"
#include <memory>
#include <string>
#include <vector>

namespace kairo {

class Runtime;

// JsAppStore — process-wide registry of JS custom apps shown in the Apps list.
// Holds the built-in (embedded) apps AND any OTA-installed ones. Installing is
// FILESYSTEM-FREE: an app pushed over the wire is registered live in RAM and
// appears immediately (volatile — lost on reboot). Persistent install (survives
// reboot) only needs a flash filesystem (SPIFFS/LittleFS on internal flash) — a
// microSD is NOT required; SD is only for bulk/removable libraries (Plan 37).
class JsAppStore {
public:
    static JsAppStore& instance();

    // Register + load a JS app live (appears in Apps now). Replaces an existing
    // app with the same id. Returns false on bad input.
    bool registerApp(Runtime& rt, std::string id, std::string name, std::string js);

    // Install from a .kapp container ("KAPP1\n<manifest-json>\n<js>"): parses the
    // id/name from the manifest, then registerApp(). This is the OTA entry point.
    bool installKapp(Runtime& rt, const char* kappBytes, size_t len);

    int count() const { return (int)apps_.size(); }

private:
    std::vector<std::unique_ptr<JsAppPlugin>> apps_;
};

// Convenience: register all built-in JS apps (embedded store, Plan 37 Fase 5).
void loadEmbeddedJsApps(Runtime& rt);

} // namespace kairo
