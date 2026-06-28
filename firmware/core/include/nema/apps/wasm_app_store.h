#pragma once
#include "nema/apps/wasm_app.h"
#include "nema/app/app_manifest.h"
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace nema {

class Runtime;

// WasmAppStore — the custom-app store for WebAssembly bundles, mirroring
// JsAppStore (Plan 84). Owns installed WASM apps and registers each into the
// AppRegistry as AppKind::Custom so it appears in the launcher alongside C and
// JS apps. WASM apps are CLI-only for now (UI is Plan 84 Fase 4).
class WasmAppStore {
public:
    static WasmAppStore& instance();

    // Build + install a WASM app live. Refuses a duplicate id. Returns false on
    // bad input or duplicate. `wasm` is the raw module bytes (owned by the app).
    bool installApp(Runtime& rt, std::string id, std::string name,
                    std::string version, std::vector<uint8_t> wasm,
                    std::string displayServer = "",
                    std::vector<std::string> args = {},
                    std::vector<uint8_t> iconData = {},
                    std::string category = "");

    int count() const { return (int)apps_.size(); }

private:
    std::list<std::unique_ptr<WasmApp>> apps_;   // list: stable pointers
};

} // namespace nema
