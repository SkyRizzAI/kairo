#pragma once
#include "nema/ui/surface.h"
#include <cstddef>
#include <cstdint>
#include <functional>
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
    ProcessContext* ctx     = nullptr;
    // Optional surface for canvas_*/ui_* ABI (Plan 86 Fase 2/3). Null for
    // headless (ProcessHost) runs. Set by WasmEngine::runStart with the
    // ISurface& provided by the caller (AppContext is ISurface + ProcessContext).
    ISurface*       surface = nullptr;
    std::string     appId;
    // Optional hook called by nema_print() in addition to rt.log().
    // Used by WasmApp to capture terminal output for the UI screen.
    std::function<void(const std::string&)> printHook;
};

class WasmEngine {
public:
    WasmEngine();
    ~WasmEngine();

    WasmEngine(const WasmEngine&) = delete;
    WasmEngine& operator=(const WasmEngine&) = delete;

    // Allocate the wasm3 environment and runtime. stackBytes comes from app config.
    // memQuotaBytes caps memory.grow (0 = no cap — wasm3 is bounded by host RAM).
    // The default is 0 (uncapped): the WASM binary's own initial-page declaration
    // determines how much linear memory is allocated at load time. A non-zero value
    // is a hard ceiling enforced by ResizeMemory; set it only when the manifest
    // declares an explicit mem_quota_bytes.
    bool init(size_t stackBytes, size_t memQuotaBytes = 0);

    // Parse + load a .wasm module.
    bool load(const uint8_t* wasm, size_t len);

    // Find and call the "_start" / "main" function. Links WASI + nema imports
    // first. appId namespaces this app's storage; nullptr/"" disables it.
    // surface: optional ISurface for canvas_*/ui_* ABI (Plan 86 Fase 2/3).
    // Returns the process exit code, or -1 on error.
    int  runStart(ProcessContext& ctx, const char* appId = nullptr,
                  ISurface* surface = nullptr);

    // Optional hook: called by nema_print() with each printed line, in addition
    // to the normal rt.log() output. Set before calling runStart().
    void setPrintHook(std::function<void(const std::string&)> hook) {
        printHook_ = std::move(hook);
    }

    // Parse + load module and validate it. Returns nullptr on error.
    // The module is owned by the runtime after m3_LoadModule.
    bool loaded() const { return mod_ != nullptr; }

    const std::string& lastError() const { return err_; }

    // ── Watchdog (Plan 87 Fase 6) ──────────────────────────────────────────
    // requestAbort: signal the currently executing VM to stop at the next
    // function-call boundary (m3_Yield hook). Thread-safe and idempotent.
    // clearAbort:   reset the flag (called internally at runStart()).
    // forceQuit (App level): see IApp::requestAbort().
    static void requestAbort();
    static void clearAbort();

private:
    M3Environment* env_ = nullptr;
    M3Runtime*     rt_  = nullptr;
    M3Module*      mod_ = nullptr;
    std::string    err_;
    WasmHostCtx    host_;     // outlives the run; referenced by m3 userdata
    std::function<void(const std::string&)> printHook_;
};

// Link host imports to the WasmHostCtx already set as the runtime userdata.
// Called by WasmEngine::runStart() before executing the guest.
//   wasi:   minimal WASI surface (fd 0/1/2, argv, exit).
//   nema:   system API (log, device info, app storage, argv).
//   canvas: raw drawing surface (Plan 86 Fase 2).
//   ui:     retained-mode UI widgets (Plan 86 Fase 3).
void linkWasiImports(struct M3Module* mod);
void linkNemaImports(struct M3Module* mod);
void linkCanvasImports(struct M3Module* mod);
void linkUiImports(struct M3Module* mod);
void linkInputImports(struct M3Module* mod);
void linkWifiImports(struct M3Module* mod);
// env:    libc bulk-memory primitives (memset/memcpy/memmove) that LLVM emits as
//         imports when an app doesn't statically bundle them (Plan 88).
void linkEnvImports(struct M3Module* mod);

// Reset per-run UI state between WASM runs (call before each new run).
void resetUiState();

} // namespace nema
