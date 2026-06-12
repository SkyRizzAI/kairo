#include "nema/js/js_engine.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/system/capability_registry.h"
#include "nema/system/system_info.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include "nema/hal/http_client.h"
#include "nema/services/profile_service.h"
#include <string>
#include <utility>

// The `nema` system API exposed to custom JS apps (Plan 37 Fase 4). Host C
// functions retrieve the engine (and thus Runtime) via the context opaque. All
// capability-gated: a method is only present if the board supports it. Blocking
// calls (http) run on the app thread (off the UI thread) — fine for apps.
namespace nema::js {

static JsEngine* self(JSContext* ctx) { return static_cast<JsEngine*>(JS_GetContextOpaque(ctx)); }

static std::string argStr(JSContext* ctx, JSValueConst v) {
    std::string r; const char* s = JS_ToCString(ctx, v);
    if (s) { r = s; JS_FreeCString(ctx, s); }
    return r;
}

// nema.log(level, tag, msg)
static JSValue api_log(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = self(ctx); if (!e || !e->host() || argc < 3) return JS_UNDEFINED;
    std::string lvl = argStr(ctx, argv[0]), tag = argStr(ctx, argv[1]), msg = argStr(ctx, argv[2]);
    auto& log = e->host()->log();
    if      (lvl == "error") log.error(tag.c_str(), msg.c_str());
    else if (lvl == "warn")  log.warn (tag.c_str(), msg.c_str());
    else if (lvl == "debug") log.debug(tag.c_str(), msg.c_str());
    else if (lvl == "trace") log.trace(tag.c_str(), msg.c_str());
    else                     log.info (tag.c_str(), msg.c_str());
    return JS_UNDEFINED;
}

// nema.device.has(cap)
static JSValue api_has(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = self(ctx); if (!e || !e->host() || argc < 1) return JS_FALSE;
    return JS_NewBool(ctx, e->host()->capabilities().has(argStr(ctx, argv[0])));
}

// nema.storage.get/set/remove — per-app namespace in the config store.
static JSValue api_store_get(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = self(ctx); if (!e || !e->host() || argc < 1) return JS_NULL;
    std::string v;
    if (e->host()->config().getString(e->appId().c_str(), argStr(ctx, argv[0]).c_str(), v))
        return JS_NewString(ctx, v.c_str());
    return JS_NULL;
}
static JSValue api_store_set(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = self(ctx); if (!e || !e->host() || argc < 2) return JS_UNDEFINED;
    e->host()->config().setString(e->appId().c_str(), argStr(ctx, argv[0]).c_str(), argStr(ctx, argv[1]));
    return JS_UNDEFINED;
}
static JSValue api_store_remove(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = self(ctx); if (!e || !e->host() || argc < 1) return JS_UNDEFINED;
    e->host()->config().remove(e->appId().c_str(), argStr(ctx, argv[0]).c_str());
    return JS_UNDEFINED;
}

// nema.profile.* — read-only identity + verify API for custom apps (Plan 40).
// Setters are intentionally absent: apps cannot change the owner's identity.
static JSValue api_profile_userName(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* e = self(ctx); if (!e || !e->host()) return JS_NULL;
    auto* p = e->host()->container().resolve<ProfileService>();
    return p ? JS_NewString(ctx, p->userName().c_str()) : JS_NULL;
}
static JSValue api_profile_deviceName(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* e = self(ctx); if (!e || !e->host()) return JS_NULL;
    auto* p = e->host()->container().resolve<ProfileService>();
    return p ? JS_NewString(ctx, p->deviceName().c_str()) : JS_NULL;
}
static JSValue api_profile_hasPassword(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* e = self(ctx); if (!e || !e->host()) return JS_FALSE;
    auto* p = e->host()->container().resolve<ProfileService>();
    return p ? JS_NewBool(ctx, p->hasPassword()) : JS_FALSE;
}
static JSValue api_profile_verifyPassword(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = self(ctx); if (!e || !e->host() || argc < 1) return JS_FALSE;
    auto* p = e->host()->container().resolve<ProfileService>();
    return p ? JS_NewBool(ctx, p->verifyPassword(argStr(ctx, argv[0]))) : JS_FALSE;
}

// nema.http.get(url) → { status, body }. Blocking on the app thread (off UI).
static JSValue api_http_get(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = self(ctx); if (!e || !e->host() || argc < 1) return JS_NULL;
    auto* client = e->host()->container().resolve<IHttpClient>();
    if (!client) return JS_ThrowTypeError(ctx, "http not available");
    HttpResponse r = client->get(argStr(ctx, argv[0]).c_str());
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "status", JS_NewInt32(ctx, r.status));
    JS_SetPropertyStr(ctx, o, "body",   JS_NewString(ctx, r.body.c_str()));
    return o;   // `await` unwraps the plain object too
}

static void setFn(JSContext* ctx, JSValue obj, const char* name, JSCFunction* fn, int argc) {
    JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, fn, name, argc));
}

void JsEngine::setHost(nema::Runtime* rt, std::string appId) {
    host_  = rt;
    appId_ = std::move(appId);
    if (ok()) installApi();
}

void JsEngine::installApi() {
    if (!host_) return;
    JSContext* ctx = ctx_;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue api = JS_NewObject(ctx);

    setFn(ctx, api, "log", api_log, 3);

    // device { name, caps[], has() }
    JSValue dev = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, dev, "name", JS_NewString(ctx, host_->info().boardName.c_str()));
    const auto& caps = host_->capabilities().list();
    JSValue arr = JS_NewArray(ctx);
    for (uint32_t i = 0; i < caps.size(); i++)
        JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, caps[i].c_str()));
    JS_SetPropertyStr(ctx, dev, "caps", arr);
    setFn(ctx, dev, "has", api_has, 1);
    JS_SetPropertyStr(ctx, api, "device", dev);

    // storage (always available — config store)
    JSValue st = JS_NewObject(ctx);
    setFn(ctx, st, "get", api_store_get, 1);
    setFn(ctx, st, "set", api_store_set, 2);
    setFn(ctx, st, "remove", api_store_remove, 1);
    JS_SetPropertyStr(ctx, api, "storage", st);

    // http (gated): present only if the board can do networked requests.
    if (host_->capabilities().has("http") || host_->capabilities().has("wifi")) {
        JSValue http = JS_NewObject(ctx);
        setFn(ctx, http, "get", api_http_get, 1);
        JS_SetPropertyStr(ctx, api, "http", http);
    }

    // profile (always present when capability "profile" is available — safe to
    // expose unconditionally: functions return null/false if service is absent)
    if (host_->capabilities().has("profile") ||
        host_->container().resolve<ProfileService>()) {
        JSValue prof = JS_NewObject(ctx);
        setFn(ctx, prof, "userName",       api_profile_userName,       0);
        setFn(ctx, prof, "deviceName",     api_profile_deviceName,     0);
        setFn(ctx, prof, "hasPassword",    api_profile_hasPassword,    0);
        setFn(ctx, prof, "verifyPassword", api_profile_verifyPassword, 1);
        JS_SetPropertyStr(ctx, api, "profile", prof);
    }

    JS_SetPropertyStr(ctx, g, "nema", api);
    JS_FreeValue(ctx, g);
}

} // namespace nema::js
