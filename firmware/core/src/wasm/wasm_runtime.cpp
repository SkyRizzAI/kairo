// Plan 57 — WasmRuntime implementation.
#include "nema/wasm/wasm_runtime.h"
#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"

namespace nema {

void WasmRuntime::runProcess(const AppManifest& m, const char* bundle, ProcessContext& ctx) {
    if (!bundle) return;

    // The bundle pointer is the raw .wasm bytes. The loader passes the data
    // that was read from the .wasm file or .papp container.
    // For now, we use the bundle as the wasm data pointer.
    // Phase 3 integration: the manifest tells us the bundle size.

    WasmEngine engine;
    if (!engine.init(8192)) {  // 8 KB stack default
        ctx.runtime().log().error("WasmRuntime", "init failed",
                                  {{"app", m.id}, {"err", engine.lastError()}});
        return;
    }

    // bundle = raw wasm bytes; size from manifest metadata
    // TODO Phase 3: store bundle size in AppManifest or process spec
    if (!engine.load(reinterpret_cast<const uint8_t*>(bundle), 1024)) {
        ctx.runtime().log().error("WasmRuntime", "load failed",
                                  {{"app", m.id}, {"err", engine.lastError()}});
        return;
    }

    int exitCode = engine.runStart(ctx);
    ctx.requestExit(exitCode);
}

} // namespace nema
