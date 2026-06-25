#include "nema/apps/js_app_store.h"
#include "nema/app/api_version.h"
#include "nema/app/app_manifest.h"
#include "nema/app/runtime_tier.h"
#include "nema/apps/embedded_apps.h"
#include "nema/app/app_registry.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <nlohmann/json.hpp>
#include <utility>
#include <algorithm>

namespace nema {

JsAppStore& JsAppStore::instance() {
    static JsAppStore s;
    return s;
}

bool JsAppStore::installApp(Runtime& rt, std::string id, std::string name,
                            std::string version, std::string js,
                            std::string displayServer,
                            std::vector<std::string> args,
                            std::vector<uint8_t> iconData) {
    if (id.empty() || js.empty()) return false;
    // Re-install with a known id → REPLACE in place so `palanu cp` of a new build
    // swaps the bytes instead of vanishing the app until reboot (same fix as
    // WasmAppStore — see there).
    apps_.erase(std::remove_if(apps_.begin(), apps_.end(),
                    [&](const std::unique_ptr<JsApp>& p) { return id == p->id(); }),
                apps_.end());
    if (version.empty()) version = "1.0.0";
    apps_.push_back(std::make_unique<JsApp>(std::move(id), std::move(name),
                                            std::move(version), std::move(js),
                                            std::move(displayServer)));
    JsApp& app = *apps_.back();

    if (!iconData.empty()) app.setIcon(std::move(iconData));

    // Plan 56/59 — build a full manifest that carries tier + display server.
    AppManifest m;
    m.id            = app.id();
    m.name          = app.name();
    m.version       = app.version();
    m.runtimeTier   = RuntimeTier::Js;
    m.displayServer = app.displayServer();   // nullptr if not set
    m.args          = std::move(args);
    m.iconBitmap    = app.iconBitmap();      // nullptr if no custom icon
    m.iconW         = app.iconW();
    m.iconH         = app.iconH();
    rt.apps().installCustom(app, m);         // appears in the launcher now
    return true;
}

// Parse a "major.minor" version string from the manifest, returning 0 for
// both fields on parse failure (backward-compat: pre-v1 manifests are
// accepted in this release).
static std::pair<uint16_t, uint16_t> parseApiVersion(const std::string& s) {
    auto dot = s.find('.');
    if (dot == std::string::npos) return {0, 0};
    try {
        uint16_t major = static_cast<uint16_t>(std::stoul(s.substr(0, dot)));
        uint16_t minor = static_cast<uint16_t>(std::stoul(s.substr(dot + 1)));
        return {major, minor};
    } catch (...) {
        return {0, 0};
    }
}

bool JsAppStore::installPappBytes(Runtime& rt, const char* bytes, size_t len) {
    std::string s(bytes, len);

    // Line 0: magic ("PAPP1")
    auto nl1 = s.find('\n');
    if (nl1 == std::string::npos) return false;
    std::string magic = s.substr(0, nl1);
    if (magic != "PAPP1") {
        rt.log().error("JsAppStore", "unknown bundle magic", {{"magic", magic}});
        return false;
    }

    // Line 1: manifest JSON
    auto nl2 = s.find('\n', nl1 + 1);
    if (nl2 == std::string::npos) return false;
    std::string manifestStr = s.substr(nl1 + 1, nl2 - nl1 - 1);

    // Line 2: entry filename (skip it — we only use the JS bundle bytes)
    auto nl3 = s.find('\n', nl2 + 1);
    if (nl3 == std::string::npos) return false;
    std::string js = s.substr(nl3 + 1);

    // Parse manifest as full JSON (Plan 48 Fase 3 — nested fields needed).
    auto m = nlohmann::json::parse(manifestStr, nullptr, false);
    if (m.is_discarded()) {
        rt.log().error("JsAppStore", "bad manifest JSON");
        return false;
    }

    std::string id            = m.value("id", "");
    std::string name          = m.value("name", "");
    std::string version       = m.value("version", "1.0.0");
    std::string displayServer = m.value("display_server", "");  // Plan 51/59
    if (id.empty()) return false;
    if (name.empty()) name = id;

    // API version check (Plan 48 Fase 3).
    //   major must match exactly (incompatible ABI).
    //   app.minor ≤ host.minor (app must not need functions that don't exist).
    std::string apiStr = m.value("api_version", "");
    if (!apiStr.empty()) {
        auto [appMajor, appMinor] = parseApiVersion(apiStr);
        if (appMajor == 0 && appMinor == 0) {
            rt.log().warn("JsAppStore", "unparseable api_version, allowing",
                          {{"app", id}, {"api_version", apiStr}});
        } else if (appMajor != NEMA_API_VERSION_MAJOR) {
            rt.log().error("JsAppStore", "refused: incompatible API major",
                           {{"app", id},
                            {"app_major", std::to_string(appMajor)},
                            {"host_major", std::to_string(NEMA_API_VERSION_MAJOR)}});
            return false;
        } else if (appMinor > NEMA_API_VERSION_MINOR) {
            rt.log().error("JsAppStore", "refused: app needs newer API minor",
                           {{"app", id},
                            {"app_minor", std::to_string(appMinor)},
                            {"host_minor", std::to_string(NEMA_API_VERSION_MINOR)}});
            return false;
        }
    }
    // Missing or empty api_version → accepted (backward-compat with pre-v1 apps).

    return installApp(rt, std::move(id), std::move(name), std::move(version),
                      std::move(js), std::move(displayServer));
}

void loadEmbeddedJsApps(Runtime& rt) {
    for (int i = 0; i < EMBEDDED_APP_COUNT; i++) {
        const auto& a = EMBEDDED_APPS[i];
        JsAppStore::instance().installApp(rt, a.id, a.name, "1.0.0", a.js);
    }
}

} // namespace nema
