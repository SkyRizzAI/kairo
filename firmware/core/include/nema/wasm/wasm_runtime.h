#pragma once
#include "nema/app/app_runtime.h"
#include "nema/app/app_manifest.h"
#include "nema/app/runtime_tier.h"

// Plan 57 — WasmRuntime: IAppRuntime adapter for the wasm3 interpreter tier.
// runProcess() loads a .wasm bundle, spins up WasmEngine, links WASI
// imports to ProcessContext, and calls _start.

namespace nema {

class ProcessContext;

class WasmRuntime : public IAppRuntime {
public:
    const char* tierName() const override { return "wasm"; }

    bool canHandle(const AppManifest& m) const override {
        return m.runtimeTier == RuntimeTier::Wasm;
    }

    // Headless process: load .wasm → WasmEngine → WASI → _start.
    void runProcess(const AppManifest& m, const char* bundle, ProcessContext& ctx) override;

    // UI surface: deferred to Phase 5 (aether:ui surface import).
    void runUi(const AppManifest&, const char*, AppContext&) override {}
};

} // namespace nema
