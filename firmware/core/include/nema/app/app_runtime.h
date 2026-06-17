#pragma once

// Plan 56 — IAppRuntime: the adapter tier interface.
//
// Each runtime tier implements IAppRuntime. The loader uses this interface to
// launch an app without knowing which tier (native/JS/WASM) it runs in.
//
// Adapter pattern per tier:
//   CBuiltin: CBuiltinRuntime (builtin_runtime.h) — identity adapter; built-in
//             apps are already IApp pointers registered via AppRegistry::install().
//   Js:       JsRuntime (js/js_runtime.h) creates JsEngine + evaluates the bundle.
//   Wasm:     WasmRuntime (Plan 57) spins up wasm3 + maps WASI imports (deferred).
//
// Apps do NOT implement IAppRuntime. IApp (C built-in) and bundle files
// (JS/WASM) are the app-author surface; IAppRuntime is an OS-level concern.

namespace nema {

class AppContext;
class ProcessContext;
struct AppManifest;

class IAppRuntime {
public:
    virtual ~IAppRuntime() = default;

    // Short tier name: "native", "js", "wasm". Shown by `ps`.
    virtual const char* tierName() const = 0;

    // True if this runtime can handle the given manifest's tier + bundle.
    virtual bool canHandle(const AppManifest& m) const = 0;

    // Launch a UI app on the calling (app) thread. Block until exit.
    // ctx is the AppHost surface: canvas, present, input mailbox.
    virtual void runUi(const AppManifest& m, const char* bundle, AppContext& ctx);

    // Launch a headless process on the calling thread. Block until exit.
    // ctx is the stdio/args/cwd/exit surface (ProcessHost).
    virtual void runProcess(const AppManifest& m, const char* bundle, ProcessContext& ctx);
};

} // namespace nema
