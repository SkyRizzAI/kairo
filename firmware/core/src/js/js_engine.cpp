#include "nema/js/js_engine.h"
#include "nema/js/nema_runtime_js.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/ui/node.h"
#include "nema/ui/widgets.h"
#include <chrono>
#include <cstring>
#include <deque>

namespace nema::js {

using namespace nema::ui;

static uint64_t nowMs() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Per-frame stable string + scroll storage (UiNode.text is non-owning; scroll
// state must persist across frames). Kept in a deque for stable addresses.
namespace { thread_local std::deque<std::string> g_textPool; }

// ── interrupt + schedule trampolines ───────────────────────────────────────
static int interrupt_trampoline(JSRuntime*, void* opaque) {
    return static_cast<JsEngine*>(opaque)->interruptCheck();
}
static JSValue schedule_cfn(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* eng = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
    if (eng) eng->markDirty();
    return JS_UNDEFINED;
}

int JsEngine::interruptCheck() {
    if (deadlineMs_ == 0) return 0;
    return (nowMs() - startMs_) > deadlineMs_ ? 1 : 0;
}

// ── module loader: resolve `nema` / `nema/jsx-runtime` → embedded runtime ──
static char* module_normalize(JSContext* ctx, const char*, const char* name, void*) {
    return js_strdup(ctx, name);
}
static JSModuleDef* module_loader(JSContext* ctx, const char* name, void* opaque) {
    auto* self = static_cast<JsEngine*>(opaque);
    if (std::strcmp(name, "nema") == 0 || std::strcmp(name, "nema/jsx-runtime") == 0 ||
        std::strcmp(name, "nema/jsx-dev-runtime") == 0) {
        if (self->nemaModuleDef()) return self->nemaModuleDef();
        JSValue v = JS_Eval(ctx, NEMA_RUNTIME_JS, std::strlen(NEMA_RUNTIME_JS),
                            "nema", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(v)) return nullptr;
        JSModuleDef* def = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(v));
        JS_FreeValue(ctx, v);   // module is registered in the runtime now
        self->setNemaModuleDef(def);
        return def;
    }
    JS_ThrowReferenceError(ctx, "module '%s' not found (only 'nema' is provided)", name);
    return nullptr;
}

// ── lifecycle ───────────────────────────────────────────────────────────────
JsEngine::JsEngine() {
    rt_ = JS_NewRuntime();
    if (!rt_) return;
    ctx_ = JS_NewContext(rt_);
    if (!ctx_) { JS_FreeRuntime(rt_); rt_ = nullptr; return; }
    JS_SetContextOpaque(ctx_, this);
    JS_SetInterruptHandler(rt_, interrupt_trampoline, this);
    JS_SetModuleLoaderFunc(rt_, module_normalize, module_loader, this);
    // The recursion guard (JS_SetMaxStackSize) is set by the owner (JsApp) once it
    // knows the thread stack — see JsApp::onStart. QuickJS's 1 MB default is left
    // in place only until then; nothing deep runs before the owner overrides it.
    scheduleFn_ = JS_NewCFunction(ctx_, schedule_cfn, "schedule", 0);
}

JsEngine::~JsEngine() {
    if (ctx_) {
        freeHandlers();
        JS_FreeValue(ctx_, scheduleFn_);
        JS_FreeValue(ctx_, renderFn_);
        JS_FreeValue(ctx_, appComponent_);
        JS_FreeContext(ctx_);
    }
    if (rt_) JS_FreeRuntime(rt_);
}

void JsEngine::setMemoryLimit(size_t bytes) { if (rt_ && bytes) JS_SetMemoryLimit(rt_, bytes); }
void JsEngine::setMaxStackSize(size_t bytes) { if (rt_) JS_SetMaxStackSize(rt_, bytes); }
void JsEngine::setDeadlineMs(uint32_t ms)   { deadlineMs_ = ms; }
bool JsEngine::takeDirty()                  { bool d = dirty_; dirty_ = false; return d; }

void JsEngine::captureError() {
    JSValue exc = JS_GetException(ctx_);
    const char* s = JS_ToCString(ctx_, exc);
    err_ = s ? s : "(unknown error)";
    if (s) JS_FreeCString(ctx_, s);
    JSValue stk = JS_GetPropertyStr(ctx_, exc, "stack");
    if (!JS_IsUndefined(stk)) {
        const char* st = JS_ToCString(ctx_, stk);
        if (st && st[0]) { err_ += "\n"; err_ += st; }
        if (st) JS_FreeCString(ctx_, st);
    }
    JS_FreeValue(ctx_, stk);
    JS_FreeValue(ctx_, exc);
}

void JsEngine::pumpJobs() {
    JSContext* pctx;
    while (JS_ExecutePendingJob(rt_, &pctx) > 0) { /* microtasks */ }
}

bool JsEngine::settleEval(JSValue res) {
    if (JS_IsException(res)) { captureError(); JS_FreeValue(ctx_, res); return false; }
    pumpJobs();   // let the module-eval promise settle
    bool ok = true;
    if (JS_IsObject(res) && JS_PromiseState(ctx_, res) == JS_PROMISE_REJECTED) {
        JSValue reason = JS_PromiseResult(ctx_, res);
        JS_Throw(ctx_, reason);   // make captureError() read this as the exception
        captureError();
        ok = false;
    }
    JS_FreeValue(ctx_, res);
    return ok;
}

bool JsEngine::eval(const char* code, const char* filename, bool asModule) {
    if (!ok()) return false;
    err_.clear();
    startMs_ = nowMs();
    int flags = asModule ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;
    JSValue v = JS_Eval(ctx_, code, std::strlen(code), filename, flags);
    if (JS_IsException(v)) { captureError(); JS_FreeValue(ctx_, v); return false; }
    pumpJobs();
    JS_FreeValue(ctx_, v);
    return true;
}

// Compile + evaluate the embedded `nema` runtime module up-front, at shallow C
// stack depth. Without this, the module loader compiles the (large, deeply-nested
// minified) runtime *while already deep inside the app module's instantiation* —
// so the peak native stack during loadApp is app-depth + runtime-parse-depth.
// Doing it here collapses that to max(runtime-parse, app-depth), which is what
// makes constrained stacks (WASM workers, ESP FreeRTOS task) survive.
bool JsEngine::preloadRuntime() {
    if (!ok() || nemaDef_) return nemaDef_ != nullptr;
    err_.clear();
    startMs_ = nowMs();
    JSValue mod = JS_Eval(ctx_, NEMA_RUNTIME_JS, std::strlen(NEMA_RUNTIME_JS),
                          "nema", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(mod)) { captureError(); JS_FreeValue(ctx_, mod); return false; }
    nemaDef_ = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(mod));
    JSValue res = JS_EvalFunction(ctx_, JS_DupValue(ctx_, mod));   // instantiate+eval (shallow)
    JS_FreeValue(ctx_, mod);
    if (!settleEval(res)) { nemaDef_ = nullptr; return false; }
    return true;
}

bool JsEngine::loadApp(const char* js, const char* name) {
    if (!ok()) return false;
    err_.clear();
    startMs_ = nowMs();
    preloadRuntime();   // shallow-compile the runtime before descending into the app
    if (host_) host_->log().debug("JsEngine", "compile", {{"app", name}});
    // Compile the app as a module, then evaluate the graph (resolves `nema`).
    JSValue mod = JS_Eval(ctx_, js, std::strlen(js), name,
                          JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(mod)) { captureError(); JS_FreeValue(ctx_, mod); return false; }
    JSModuleDef* appDef = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(mod));
    if (host_) host_->log().debug("JsEngine", "eval", {{"app", name}});
    JSValue res = JS_EvalFunction(ctx_, JS_DupValue(ctx_, mod));
    if (!settleEval(res)) { JS_FreeValue(ctx_, mod); return false; }
    if (host_) host_->log().debug("JsEngine", "jobs", {{"app", name}});

    JSValue ns = JS_GetModuleNamespace(ctx_, appDef);
    appComponent_ = JS_GetPropertyStr(ctx_, ns, "default");
    JS_FreeValue(ctx_, ns);
    JS_FreeValue(ctx_, mod);
    if (!JS_IsFunction(ctx_, appComponent_)) { err_ = "app has no default-exported component"; return false; }

    if (nemaDef_) {
        JSValue kns = JS_GetModuleNamespace(ctx_, nemaDef_);
        renderFn_ = JS_GetPropertyStr(ctx_, kns, "renderToTree");
        JS_FreeValue(ctx_, kns);
    }
    if (!JS_IsFunction(ctx_, renderFn_)) { err_ = "nema runtime missing renderToTree"; return false; }
    return true;
}

// ── helpers to read JS node-desc props ──────────────────────────────────────
static bool getBool(JSContext* c, JSValueConst o, const char* k, bool def) {
    JSValue v = JS_GetPropertyStr(c, o, k); bool r = def;
    if (!JS_IsUndefined(v) && !JS_IsNull(v)) r = JS_ToBool(c, v);
    JS_FreeValue(c, v); return r;
}
static int getInt(JSContext* c, JSValueConst o, const char* k, int def) {
    JSValue v = JS_GetPropertyStr(c, o, k); int r = def;
    if (JS_IsNumber(v)) { int32_t t; if (!JS_ToInt32(c, &t, v)) r = t; }
    JS_FreeValue(c, v); return r;
}
static std::string getStr(JSContext* c, JSValueConst o, const char* k) {
    JSValue v = JS_GetPropertyStr(c, o, k); std::string r;
    if (JS_IsString(v)) { const char* s = JS_ToCString(c, v); if (s) { r = s; JS_FreeCString(c, s); } }
    JS_FreeValue(c, v); return r;
}

static void applyStyle(JSContext* c, JSValueConst styleObj, Style& s) {
    if (!JS_IsObject(styleObj)) return;
    std::string dir = getStr(c, styleObj, "flexDirection");
    if (dir == "row") s.dir = FlexDir::Row; else if (dir == "column") s.dir = FlexDir::Col;
    s.flexGrow = (uint16_t)getInt(c, styleObj, "flexGrow", s.flexGrow);
    s.padding  = (uint8_t) getInt(c, styleObj, "padding",  s.padding);
    s.gap      = (uint8_t) getInt(c, styleObj, "gap",      s.gap);
    int w = getInt(c, styleObj, "width", -1);  if (w >= 0) s.width  = (uint16_t)w;
    int h = getInt(c, styleObj, "height", -1); if (h >= 0) s.height = (uint16_t)h;
    std::string al = getStr(c, styleObj, "alignItems");
    if      (al == "center")  s.align = Align::Center;
    else if (al == "end")     s.align = Align::End;
    else if (al == "stretch") s.align = Align::Stretch;
    else if (al == "start")   s.align = Align::Start;
    std::string ju = getStr(c, styleObj, "justifyContent");
    if      (ju == "center")        s.justify = Justify::Center;
    else if (ju == "end")          s.justify = Justify::End;
    else if (ju == "space-between") s.justify = Justify::SpaceBetween;
    else if (ju == "start")        s.justify = Justify::Start;
    s.border     = getBool(c, styleObj, "border", s.border);
    s.background = getBool(c, styleObj, "background", s.background);
}

void JsEngine::freeHandlers() {
    for (auto& h : handlers_) JS_FreeValue(ctx_, h);
    handlers_.clear();
}

// Static thunk for UiNode.onPress → engine.callHandler(id).
static void handler_thunk(void* u) {
    auto* r = static_cast<JsEngine::HandlerRef*>(u);
    r->eng->callHandler(r->id);
}

UiNode* JsEngine::reify(JSValueConst node, NodeArena& arena) {
    if (!JS_IsObject(node)) return nullptr;
    std::string type = getStr(ctx_, node, "type");
    UiNode* n = arena.alloc();
    if (!n) return nullptr;

    // style
    JSValue styleV = JS_GetPropertyStr(ctx_, node, "style");
    applyStyle(ctx_, styleV, n->style);
    JS_FreeValue(ctx_, styleV);

    JSValue props = JS_GetPropertyStr(ctx_, node, "props");

    if (type == "Text" || type == "#text") {
        n->type = NodeType::Text;
        g_textPool.emplace_back(getStr(ctx_, node, "text"));
        n->text = g_textPool.back().c_str();
        std::string variant = JS_IsObject(props) ? getStr(ctx_, props, "variant") : "";
        n->role = variant == "title" ? TextRole::Title
                : variant == "caption" ? TextRole::Caption : TextRole::Body;
    } else if (type == "Pressable") {
        n->type = NodeType::Pressable;
        n->focusable = true;
        if (JS_IsObject(props)) {
            JSValue fn = JS_GetPropertyStr(ctx_, props, "onPress");
            if (JS_IsFunction(ctx_, fn)) {
                int id = (int)handlers_.size();
                handlers_.push_back(JS_DupValue(ctx_, fn));
                refs_[id] = { this, id };
                n->onPress  = handler_thunk;
                n->userdata = &refs_[id];
            }
            JS_FreeValue(ctx_, fn);
        }
    } else if (type == "Scroll") {
        n->type = NodeType::Scroll;
        if (scrollCursor_ >= (int)scrolls_.size()) scrolls_.push_back(new ScrollState());
        n->scroll = scrolls_[scrollCursor_++];
    } else if (type == "Slider") {
        n->type = NodeType::Slider;
        n->focusable = true;
        // Persist the slider value in an engine-owned int (JS owns the source of
        // truth; we mirror it each frame). Reuse the scroll cursor pattern.
        if (sliderCursor_ >= (int)sliderVals_.size()) sliderVals_.push_back(0);
        sliderVals_[sliderCursor_] = JS_IsObject(props) ? getInt(ctx_, props, "value", 0) : 0;
        n->sliderValue = &sliderVals_[sliderCursor_];
        n->sliderMin = (int16_t)(JS_IsObject(props) ? getInt(ctx_, props, "min", 0) : 0);
        n->sliderMax = (int16_t)(JS_IsObject(props) ? getInt(ctx_, props, "max", 100) : 100);
        n->sliderStep = (int16_t)(JS_IsObject(props) ? getInt(ctx_, props, "step", 1) : 1);
        sliderCursor_++;
    } else {
        n->type = NodeType::View;
    }

    JS_FreeValue(ctx_, props);

    // children
    JSValue kids = JS_GetPropertyStr(ctx_, node, "children");
    if (JS_IsArray(kids)) {
        JSValue lenV = JS_GetPropertyStr(ctx_, kids, "length");
        int32_t len = 0; JS_ToInt32(ctx_, &len, lenV); JS_FreeValue(ctx_, lenV);
        UiNode* prev = nullptr;
        for (int32_t i = 0; i < len; i++) {
            JSValue kv = JS_GetPropertyUint32(ctx_, kids, (uint32_t)i);
            UiNode* child = reify(kv, arena);
            JS_FreeValue(ctx_, kv);
            if (!child) continue;
            if (!prev) n->firstChild = child; else prev->nextSibling = child;
            prev = child;
        }
    }
    JS_FreeValue(ctx_, kids);
    return n;
}

UiNode* JsEngine::render(NodeArena& arena) {
    if (!ok() || !JS_IsFunction(ctx_, renderFn_)) return nullptr;
    err_.clear();
    startMs_ = nowMs();
    freeHandlers();
    refs_.assign(128, HandlerRef{ this, 0 });   // stable userdata slots
    g_textPool.clear();
    scrollCursor_ = 0;
    sliderCursor_ = 0;

    JSValueConst args[2] = { appComponent_, scheduleFn_ };
    JSValue tree = JS_Call(ctx_, renderFn_, JS_UNDEFINED, 2, args);
    if (JS_IsException(tree)) { captureError(); JS_FreeValue(ctx_, tree); return nullptr; }
    UiNode* root = JS_IsObject(tree) ? reify(tree, arena) : nullptr;
    JS_FreeValue(ctx_, tree);
    pumpJobs();
    return root;
}

bool JsEngine::callHandler(int id) {
    if (id < 0 || id >= (int)handlers_.size()) return false;
    startMs_ = nowMs();
    JSValue r = JS_Call(ctx_, handlers_[id], JS_UNDEFINED, 0, nullptr);
    if (JS_IsException(r)) { captureError(); JS_FreeValue(ctx_, r); return dirty_; }
    JS_FreeValue(ctx_, r);
    pumpJobs();
    return dirty_;
}

} // namespace nema::js
