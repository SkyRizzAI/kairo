#pragma once
#include "nema/app/app_runtime.h"

// Plan 58 — JsRuntime: IAppRuntime adapter for the QuickJS interpreter tier.
//
// Formalizes the existing JsApp/JsEngine stack as a first-class IAppRuntime.
// The loader can call runUi() directly with a JS bundle instead of constructing
// a JsApp object — same behaviour, clean tier interface.
//
// Status: JsApp (ComponentApp subclass) remains the primary launch path until
// AppRegistry is updated to route RuntimeTier::Js through IAppRuntime. This
// class defines the interface and provides the implementation so the migration
// can happen incrementally.

namespace nema {

class JsRuntime : public IAppRuntime {
public:
    const char* tierName() const override { return "js"; }

    // Accepts any manifest with runtimeTier == RuntimeTier::Js.
    bool canHandle(const AppManifest& m) const override;

    // Evaluate `bundle` as a QuickJS ES-module app and run it until exit.
    // Equivalent to JsApp::onStart() + ComponentApp::run() but invoked via
    // the IAppRuntime interface rather than through AppHost's IApp dispatch.
    void runUi(const AppManifest& m, const char* bundle, AppContext& ctx) override;

    // Headless JS processes are not yet supported — logs a warning and returns.
    void runProcess(const AppManifest& m, const char* bundle, ProcessContext& ctx) override;
};

} // namespace nema
