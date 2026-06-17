// Plan 57 — WasmEngine implementation.
// Thin wrapper over wasm3 C API (m3_core.h).
#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "wasm3.h"
#include "m3_env.h"

namespace nema {

WasmEngine::WasmEngine() = default;

WasmEngine::~WasmEngine() {
    if (mod_) { m3_FreeModule(mod_); mod_ = nullptr; }
    if (rt_)  { m3_FreeRuntime(rt_); rt_ = nullptr; }
    if (env_) { m3_FreeEnvironment(env_); env_ = nullptr; }
}

bool WasmEngine::init(size_t stackBytes, size_t memQuotaBytes) {
    env_ = m3_NewEnvironment();
    if (!env_) { err_ = "m3_NewEnvironment failed"; return false; }

    rt_ = m3_NewRuntime(env_, (uint32_t)stackBytes, nullptr);
    if (!rt_) { err_ = "m3_NewRuntime failed"; return false; }

    (void)memQuotaBytes;  // quota enforced in Phase 6
    return true;
}

bool WasmEngine::load(const uint8_t* wasm, size_t len) {
    if (!rt_) { err_ = "not initialized"; return false; }

    M3Result res = m3_ParseModule(env_, &mod_, wasm, (uint32_t)len);
    if (res) { err_ = res; return false; }

    res = m3_LoadModule(rt_, mod_);
    if (res) { err_ = res; m3_FreeModule(mod_); mod_ = nullptr; return false; }

    return true;
}

int WasmEngine::runStart(ProcessContext& ctx) {
    if (!rt_ || !mod_) { err_ = "not loaded"; return -1; }

    // Find the _start function
    IM3Function startFn;
    M3Result res = m3_FindFunction(&startFn, rt_, "_start");
    if (res) {
        // No _start — try a simple call with no args
        err_ = res;
        return -1;
    }

    // Link WASI imports before calling _start (Plan 57 Fase 2)
    linkWasiImports(mod_, ctx);

    res = m3_CallV(startFn);
    if (res) { err_ = res; return -1; }

    return 0;
}

} // namespace nema
