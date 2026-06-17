#pragma once
#include "nema/app/app_runtime.h"
#include "nema/app/app_manifest.h"
#include "nema/app/runtime_tier.h"

// Plan 56 — CBuiltinRuntime: IAppRuntime adapter for native C++ apps compiled
// into the firmware. This is the identity adapter: the app pointer is already
// an IApp; runUi() just calls app.run(ctx) directly with no marshalling.
//
// Built-in apps are registered via AppRegistry::install(IApp&), which stores
// the pointer in the Target table. CBuiltinRuntime is not involved in that path
// (no IApp* is recovered from a bundle here). The class exists to:
//   1. Satisfy the IAppRuntime contract so AppRegistry::launch() can route
//      through a uniform interface when it needs to.
//   2. Provide tierName() / canHandle() for the ps command.

namespace nema {

class AppContext;

class CBuiltinRuntime : public IAppRuntime {
public:
    const char* tierName() const override { return "native"; }

    bool canHandle(const AppManifest& m) const override {
        return m.runtimeTier == RuntimeTier::CBuiltin;
    }

    // Built-in apps launch via AppRegistry → AppHostManager → AppHost::enter(),
    // which calls IApp::run(AppContext&) directly. runUi() is provided here so
    // the interface is complete, but the normal path bypasses IAppRuntime.
    void runUi(const AppManifest&, const char*, AppContext&) override {}

    // Built-in headless processes: not launched via bundle; ProcessHost calls
    // IApp::run() directly through AppHost. No-op here for interface compliance.
    void runProcess(const AppManifest&, const char*, ProcessContext&) override {}
};

} // namespace nema
