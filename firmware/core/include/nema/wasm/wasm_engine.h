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

    // Find and call the "_start" function (WASI entry point).
    // Returns the exit code, or -1 on error.
    int  runStart(ProcessContext& ctx);

    // Parse + load module and validate it. Returns nullptr on error.
    // The module is owned by the runtime after m3_LoadModule.
    bool loaded() const { return mod_ != nullptr; }

    const std::string& lastError() const { return err_; }

private:
    M3Environment* env_ = nullptr;
    M3Runtime*     rt_  = nullptr;
    M3Module*      mod_ = nullptr;
    std::string    err_;
};

} // namespace nema

// Link minimal WASI imports (fd 0/1/2, argv, exit) to ProcessContext.
// Called by WasmEngine::runStart() before executing the guest.
void linkWasiImports(struct M3Module* mod, nema::ProcessContext& ctx);
