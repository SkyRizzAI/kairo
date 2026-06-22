// Plan 57 — WasmEngine implementation.
// Thin wrapper over wasm3 C API (m3_core.h).
#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "wasm3.h"
#include "m3_env.h"

namespace nema {

WasmEngine::WasmEngine() = default;

WasmEngine::~WasmEngine() {
    // After m3_LoadModule succeeds the runtime OWNS the module — m3_FreeRuntime
    // frees it. Calling m3_FreeModule here too would double-free. A module that
    // failed to load is already freed+nulled in load(), so mod_ here is either
    // null or runtime-owned: never free it directly.
    mod_ = nullptr;
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

int WasmEngine::runStart(ProcessContext& ctx, const char* appId, ISurface* surface) {
    if (!rt_ || !mod_) { err_ = "not loaded"; return -1; }

    // Attach the host context to the runtime so import trampolines can reach the
    // kernel. host_ is a member, so it outlives this call.
    host_.ctx       = &ctx;
    host_.surface   = surface;
    host_.appId     = appId ? appId : "";
    host_.printHook = printHook_;
    rt_->userdata   = &host_;

    // Link imports FIRST: m3_FindFunction triggers lazy compilation of the
    // module, which resolves import references. Unlinked imports at that point
    // fail compilation with "missing import" (Plan 57 Fase 2).
    linkWasiImports(mod_);
    linkNemaImports(mod_);
    linkCanvasImports(mod_);

    IM3Function startFn;
    M3Result res;
    bool isBare = (m3_FindFunction(&startFn, rt_, "main") == m3Err_none);
    if (isBare) {
        // Bare-metal app (Plan 85): main(int argc, char* argv[]) — call with
        // no args (argc=0, argv=NULL). Apps check argc>1 before using argv.
        res = m3_CallV(startFn, (uint32_t)0, (uint32_t)0);
    } else {
        // Legacy WASI app: _start() — no args, unwinds via proc_exit trap.
        res = m3_FindFunction(&startFn, rt_, "_start");
        if (res) { err_ = res; return -1; }
        res = m3_CallV(startFn);
    }
    // proc_exit() in WASI apps unwinds via m3Err_trapExit — that's normal.
    if (res && res != m3Err_trapExit) { err_ = res; return -1; }

    return ctx.shouldExit() ? ctx.exitCode() : 0;
}

} // namespace nema
