// js_api.cpp — Thin bridge between JsEngine and generated QuickJS bindings.
// Plan 49 Fase 2: replaces the hand-written installApi() with a call to
// generated installNemaApi(). The marshalling + registration code lives in
// generated/host/nema_api_quickjs.gen.cpp (produced by tools/idl/gen.ts).
//
// The hand-written HostApi implementation is in nema_host_impl.cpp.
// This file just wires the two together at setHost() time.

#include "nema/js/js_engine.h"
#include "nema/proc/process_context.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "host/nema_api.gen.h"
#include <cstring>

// Forward declaration from generated/host/nema_api_quickjs.gen.cpp (Plan 49).
// Defined in the generated file alongside all the function wrappers.
void installNemaApi(JSContext* ctx, HostApi* host, nema::CapabilityRegistry& caps);

// Forward declarations from nema_host_impl.cpp (hand-written).
HostApi* createNemaHost(nema::Runtime& rt, std::string appId);
HostApi* createNemaHost(nema::Runtime& rt, std::string appId, nema::AppContext* ctx);

// ── Convenience shortcuts ─────────────────────────────────────────────────────
// The generated API uses deep paths (nema.storage.kv.get, nema.sys.device.name).
// App code (and the hello example) expects flat paths: nema.storage.get,
// nema.device.name, nema.log(). These shims are added on top of the generated
// global after installNemaApi — no changes to the generated file needed.

static JSValue shim_log(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = static_cast<nema::js::JsEngine*>(JS_GetContextOpaque(ctx));
    HostApi* h = e ? e->hostApi() : nullptr;
    if (!h || argc < 3) return JS_UNDEFINED;
    const char* lv  = JS_ToCString(ctx, argv[0]);
    const char* tag = JS_ToCString(ctx, argv[1]);
    const char* msg = JS_ToCString(ctx, argv[2]);
    if (lv && tag && msg) h->log_log(lv, tag, msg);
    if (lv)  JS_FreeCString(ctx, lv);
    if (tag) JS_FreeCString(ctx, tag);
    if (msg) JS_FreeCString(ctx, msg);
    return JS_UNDEFINED;
}
static JSValue shim_kv_get(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = static_cast<nema::js::JsEngine*>(JS_GetContextOpaque(ctx));
    HostApi* h = e ? e->hostApi() : nullptr;
    if (!h || argc < 1) return JS_NULL;
    const char* k = JS_ToCString(ctx, argv[0]);
    if (!k) return JS_NULL;
    auto v = h->kv_get(k);
    JS_FreeCString(ctx, k);
    return v.has_value() ? JS_NewString(ctx, v->c_str()) : JS_NULL;
}
static JSValue shim_kv_set(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = static_cast<nema::js::JsEngine*>(JS_GetContextOpaque(ctx));
    HostApi* h = e ? e->hostApi() : nullptr;
    if (!h || argc < 2) return JS_UNDEFINED;
    const char* k = JS_ToCString(ctx, argv[0]);
    const char* v = JS_ToCString(ctx, argv[1]);
    if (k && v) h->kv_set(k, v);
    if (k) JS_FreeCString(ctx, k);
    if (v) JS_FreeCString(ctx, v);
    return JS_UNDEFINED;
}
static JSValue shim_kv_remove(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = static_cast<nema::js::JsEngine*>(JS_GetContextOpaque(ctx));
    HostApi* h = e ? e->hostApi() : nullptr;
    if (!h || argc < 1) return JS_FALSE;
    const char* k = JS_ToCString(ctx, argv[0]);
    if (!k) return JS_FALSE;
    bool ok = h->kv_remove(k);
    JS_FreeCString(ctx, k);
    return JS_NewBool(ctx, ok);
}

static void installNemaApiShims(JSContext* ctx, HostApi* host) {
    JSValue g    = JS_GetGlobalObject(ctx);
    JSValue nema = JS_GetPropertyStr(ctx, g, "nema");
    if (!JS_IsObject(nema)) { JS_FreeValue(ctx, nema); JS_FreeValue(ctx, g); return; }

    // nema.log(level, tag, msg) — flat shortcut
    JS_SetPropertyStr(ctx, nema, "log",
        JS_NewCFunction(ctx, shim_log, "log", 3));

    // nema.device.name, .caps() — flat shortcut (mirrors nema.sys.device)
    JSValue dev = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, dev, "name",
        JS_NewString(ctx, host->device_name().c_str()));
    {
        auto caps = host->device_caps();
        JSValue arr = JS_NewArray(ctx);
        for (uint32_t i = 0; i < (uint32_t)caps.size(); i++)
            JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, caps[i].c_str()));
        JS_SetPropertyStr(ctx, dev, "caps", arr);
    }
    JS_SetPropertyStr(ctx, nema, "device", dev);

    // nema.storage.get/set/remove — flat shortcuts (the generated path is .kv.*)
    JSValue storage = JS_GetPropertyStr(ctx, nema, "storage");
    if (JS_IsObject(storage)) {
        JS_SetPropertyStr(ctx, storage, "get",    JS_NewCFunction(ctx, shim_kv_get,    "get",    1));
        JS_SetPropertyStr(ctx, storage, "set",    JS_NewCFunction(ctx, shim_kv_set,    "set",    2));
        JS_SetPropertyStr(ctx, storage, "remove", JS_NewCFunction(ctx, shim_kv_remove, "remove", 1));
    }
    JS_FreeValue(ctx, storage);
    JS_FreeValue(ctx, nema);
    JS_FreeValue(ctx, g);
}

namespace nema::js {

void JsEngine::setHost(nema::Runtime* rt, std::string appId) {
    host_  = rt;
    appId_ = std::move(appId);
    if (!ok()) return;

    // Create the hand-written HostApi implementation (Plan 49 Fase 2).
    // The engine owns this pointer; it is deleted in ~JsEngine().
    setHostApi(createNemaHost(*rt, appId_));

    // Install the generated `nema` global into the JS context.
    // Gating by capability is handled by the generated code.
    installNemaApi(static_cast<JSContext*>(ctx_), hostApi_, rt->capabilities());

    // Add flat convenience shortcuts on top of the generated namespaced API.
    installNemaApiShims(static_cast<JSContext*>(ctx_), hostApi_);
}

// UI-app overload: passes AppContext so NemaHostImpl can call ctx.storage()
// instead of constructing AppStorage itself. AppContext::storage() is pre-warmed
// by AppHost::onResume() on the GUI (internal-RAM) thread before the PSRAM-stacked
// app thread starts — keeping NVS reads off the PSRAM thread entirely.
void JsEngine::setHost(nema::AppContext& ctx) {
    host_  = &ctx.runtime();
    appId_ = ctx.bundleId();
    if (!ok()) return;
    setHostApi(createNemaHost(*host_, appId_, &ctx));
    installNemaApi(static_cast<JSContext*>(ctx_), hostApi_, host_->capabilities());
    installNemaApiShims(static_cast<JSContext*>(ctx_), hostApi_);
}

// ── Plan 58 — process global ─────────────────────────────────────────────────
// Install `process` into the QuickJS context. The ProcessContext pointer is
// stored in the JSRuntime opaque so C callbacks can reach it without captures.
// Calling convention: JSContext opaque → JsEngine → proc_.

namespace {

JSValue process_exit(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* eng = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
    int32_t code = 0;
    if (argc > 0) JS_ToInt32(ctx, &code, argv[0]);
    if (eng && eng->host()) {
        // Reach the ProcessContext stored in the engine.
        nema::ProcessContext* proc = eng->proc();
        if (proc) proc->requestExit(code);
    }
    // Throw a sentinel to unwind the JS call stack cleanly.
    return JS_ThrowReferenceError(ctx, "process.exit(%ld)", (long)code);
}

JSValue stdout_write(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* eng = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
    if (argc < 1 || !eng) return JS_UNDEFINED;
    nema::ProcessContext* proc = eng->proc();
    if (!proc) return JS_UNDEFINED;
    const char* s = JS_ToCString(ctx, argv[0]);
    if (s) {
        proc->out().write(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

JSValue stdin_read(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    // Blocking stdin reads are not yet implemented for UI apps; return empty string.
    // Headless (Plan 54 pipe integration) will replace this with a real read.
    return JS_NewString(ctx, "");
}

} // anon namespace

void JsEngine::setProcessContext(nema::ProcessContext* ctx) {
    proc_ = ctx;
    if (!ok() || !ctx) return;
    JSContext* jctx = static_cast<JSContext*>(ctx_);

    // Build process.argv array.
    const auto& args = ctx->args();
    JSValue argv = JS_NewArray(jctx);
    for (size_t i = 0; i < args.size(); i++)
        JS_SetPropertyUint32(jctx, argv, (uint32_t)i, JS_NewString(jctx, args[i].c_str()));

    // Build process.stdout object.
    JSValue pstdout = JS_NewObject(jctx);
    JS_SetPropertyStr(jctx, pstdout, "write",
        JS_NewCFunction(jctx, stdout_write, "write", 1));

    // Build process.stdin object.
    JSValue pstdin = JS_NewObject(jctx);
    JS_SetPropertyStr(jctx, pstdin, "read",
        JS_NewCFunction(jctx, stdin_read, "read", 0));

    // Assemble process object.
    JSValue proc = JS_NewObject(jctx);
    JS_SetPropertyStr(jctx, proc, "argv",   argv);
    JS_SetPropertyStr(jctx, proc, "exit",   JS_NewCFunction(jctx, process_exit, "exit", 1));
    JS_SetPropertyStr(jctx, proc, "stdout", pstdout);
    JS_SetPropertyStr(jctx, proc, "stdin",  pstdin);

    // Install as a global.
    JSValue global = JS_GetGlobalObject(jctx);
    JS_SetPropertyStr(jctx, global, "process", proc);
    JS_FreeValue(jctx, global);
}

} // namespace nema::js
