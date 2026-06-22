#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Plan 57 — WasmEngine: thin wrapper around wasm3 interpreter.
// One instance per app. Lifecycle: init → load → linkHost → runStart.
// Built-in WASI is disabled; we implement it ourselves via ProcessContext.

struct M3Environment;
struct M3Runtime;
struct M3Module;

namespace nema {

class ProcessContext;

// Userdata attached to the wasm3 M3Runtime so host imports (WASI + nema) can
// reach the kernel. Stored once per run by WasmEngine::runStart(); both import
// bridges read it back via m3_GetUserData(). appId is the bundle id used to
// namespace AppStorage — it must NOT be derived from argv (a CLI invocation
// could override argv[0] and cross app-storage boundaries).
struct WasmHostCtx {
    ProcessContext* ctx   = nullptr;
    std::string     appId;
};

class WasmEngine {
public:
    WasmEngine();
    ~WasmEngine();

    WasmEngine(const WasmEngine&) = delete;
    WasmEngine& operator=(const WasmEngine&) = delete;

    // Allocate the wasm3 environment and runtime. stackBytes comes from app config.
    bool init(size_t stackBytes, size_t memQuotaBytes = 65536);

    // Parse + load a .wasm module.
    bool load(const uint8_t* wasm, size_t len);

    // Find and call the "_start" function (WASI entry point). Links WASI + nema
    // imports first. appId namespaces this app's storage; nullptr/"" disables it.
    // Returns the process exit code, or -1 on error.
    int  runStart(ProcessContext& ctx, const char* appId = nullptr);

    // Parse + load module and validate it. Returns nullptr on error.
    // The module is owned by the runtime after m3_LoadModule.
    bool loaded() const { return mod_ != nullptr; }

    const std::string& lastError() const { return err_; }

private:
    M3Environment* env_ = nullptr;
    M3Runtime*     rt_  = nullptr;
    M3Module*      mod_ = nullptr;
    std::string    err_;
    WasmHostCtx    host_;   // outlives the run; referenced by m3 userdata
};

// Link host imports to the WasmHostCtx already set as the runtime userdata.
// Called by WasmEngine::runStart() before executing the guest.
//   wasi: minimal WASI surface (fd 0/1/2, argv, exit).
//   nema: system API (log, device info, app storage).
void linkWasiImports(struct M3Module* mod);
void linkNemaImports(struct M3Module* mod);

} // namespace nema
