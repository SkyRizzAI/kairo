#include "nema/js/js_runtime.h"
#include "nema/app/app_manifest.h"
#include "nema/app/app_runtime.h"
#include "nema/app/app_context.h"
#include "nema/app/runtime_tier.h"
#include "nema/apps/js_app.h"
#include "nema/proc/process_context.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/ui/display_server.h"
#include "nema/ui/ui_sdk.h"

namespace nema {

// IAppRuntime default stubs (defined here to avoid a separate TU) ─────────────
void IAppRuntime::runUi(const AppManifest&, const char*, AppContext&) {}
void IAppRuntime::runProcess(const AppManifest&, const char*, ProcessContext&) {}

// JsRuntime ───────────────────────────────────────────────────────────────────

bool JsRuntime::canHandle(const AppManifest& m) const {
    return m.runtimeTier == RuntimeTier::Js;
}

void JsRuntime::runUi(const AppManifest& m, const char* bundle, AppContext& ctx) {
    Runtime& rt = ctx.runtime();

    // Plan 50 — log which UI SDK the active display server exposes.
    if (const IDisplayServer* srv = rt.displayServer()) {
        if (const UiSdkDescriptor* sdk = srv->uiSdk()) {
            rt.log().info("JsRuntime", "ui sdk",
                {{"app", m.id}, {"sdk", sdk->ns}, {"server", srv->name()}});
        }
    }

    // Delegate to the existing JsApp + ComponentApp run loop — no duplication.
    JsApp app(
        m.id    ? m.id    : "",
        m.name  ? m.name  : "",
        m.version ? m.version : "1.0.0",
        bundle  ? bundle  : ""
    );
    app.run(ctx);
}

void JsRuntime::runProcess(const AppManifest& m, const char*, ProcessContext& ctx) {
    ctx.runtime().log().warn("JsRuntime", "headless JS not yet supported", {{"app", m.id}});
}

} // namespace nema
