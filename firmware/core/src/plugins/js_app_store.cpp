#include "kairo/plugins/js_app_store.h"
#include "kairo/apps/embedded_apps.h"
#include "kairo/plugin/plugin_manager.h"
#include "kairo/runtime.h"
#include <utility>

namespace kairo {

JsAppStore& JsAppStore::instance() {
    static JsAppStore s;
    return s;
}

bool JsAppStore::registerApp(Runtime& rt, std::string id, std::string name, std::string js) {
    if (id.empty() || js.empty()) return false;
    for (auto& p : apps_) if (id == p->id()) return false;   // already installed
    auto plugin = std::make_unique<JsAppPlugin>(id, std::move(name), std::move(js));
    rt.plugins().load(*plugin);   // appears in the Apps list immediately
    apps_.push_back(std::move(plugin));
    return true;
}

// Extract a string field value from a flat manifest JSON line: "key":"value".
static std::string jsonField(const std::string& json, const char* key) {
    std::string needle = std::string("\"") + key + "\"";
    auto k = json.find(needle);
    if (k == std::string::npos) return {};
    auto colon = json.find(':', k + needle.size());
    if (colon == std::string::npos) return {};
    auto q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) return {};
    auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return {};
    return json.substr(q1 + 1, q2 - q1 - 1);
}

bool JsAppStore::installKapp(Runtime& rt, const char* bytes, size_t len) {
    std::string s(bytes, len);
    auto nl1 = s.find('\n');
    if (nl1 == std::string::npos) return false;
    auto nl2 = s.find('\n', nl1 + 1);
    if (nl2 == std::string::npos) return false;
    std::string manifest = s.substr(nl1 + 1, nl2 - nl1 - 1);
    std::string js       = s.substr(nl2 + 1);
    std::string id   = jsonField(manifest, "id");
    std::string name = jsonField(manifest, "name");
    if (name.empty()) name = id;
    return registerApp(rt, std::move(id), std::move(name), std::move(js));
}

void loadEmbeddedJsApps(Runtime& rt) {
    for (int i = 0; i < EMBEDDED_APP_COUNT; i++) {
        const auto& a = EMBEDDED_APPS[i];
        JsAppStore::instance().registerApp(rt, a.id, a.name, a.js);
    }
}

} // namespace kairo
