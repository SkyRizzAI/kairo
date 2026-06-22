#include "nema/apps/wasm_app_store.h"
#include "nema/app/app_manifest.h"
#include "nema/app/runtime_tier.h"
#include "nema/app/app_registry.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <utility>

namespace nema {

WasmAppStore& WasmAppStore::instance() {
    static WasmAppStore s;
    return s;
}

bool WasmAppStore::installApp(Runtime& rt, std::string id, std::string name,
                              std::string version, std::vector<uint8_t> wasm,
                              std::string displayServer, AppMode mode,
                              std::vector<uint8_t> iconData) {
    if (id.empty() || wasm.empty()) return false;
    for (auto& p : apps_) if (id == p->id()) return false;   // already installed
    if (version.empty()) version = "1.0.0";

    apps_.push_back(std::make_unique<WasmApp>(std::move(id), std::move(name),
                                              std::move(version), std::move(wasm),
                                              std::move(displayServer)));
    WasmApp& app = *apps_.back();

    if (!iconData.empty()) app.setIcon(std::move(iconData));

    AppManifest m;
    m.id            = app.id();
    m.name          = app.name();
    m.version       = app.version();
    m.runtimeTier   = RuntimeTier::Wasm;
    m.displayServer = app.displayServer();
    m.mode          = mode;
    m.iconBitmap    = app.iconBitmap();
    m.iconW         = app.iconW();
    m.iconH         = app.iconH();
    rt.apps().installCustom(app, m);
    return true;
}

} // namespace nema
