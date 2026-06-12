#pragma once
#include "quickjs.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Embedded JS engine (QuickJS-ng) — Plan 37. One JSRuntime+JSContext per engine
// (one per app, on the app thread). Loads a .kapp bundle as an ES module, wires
// the embedded `nema` runtime (jsx/components/hooks/renderToTree) via a module
// loader, renders the app's default-export component to a native UiNode tree, and
// dispatches onPress/onChange handlers back into JS. quickjs.h leaks only to the
// JS-layer .cpp files (js_engine/js_app include it); the rest of the firmware
// forward-declares JsEngine.
namespace nema { class Runtime; }
namespace nema::ui { struct UiNode; class NodeArena; struct ScrollState; }

namespace nema::js {

class JsEngine {
public:
    JsEngine();
    ~JsEngine();
    JsEngine(const JsEngine&) = delete;
    JsEngine& operator=(const JsEngine&) = delete;

    bool ok() const { return rt_ && ctx_; }

    // Evaluate a script (global by default). Returns false on exception.
    bool eval(const char* code, const char* filename = "<eval>", bool asModule = false);

    // Load a custom-app bundle (ES module). Resolves `nema` imports to the
    // embedded runtime, evaluates the module, captures its default export
    // (the App component) + the runtime's renderToTree. Returns false on error.
    bool loadApp(const char* js, const char* name = "<app>");

    // Compile+evaluate the embedded `nema` runtime at shallow stack depth. Called
    // automatically by loadApp(); exposed for measurement. Idempotent.
    bool preloadRuntime();

    // Render the loaded app to a native UiNode tree (built in `arena`). Call each
    // frame from JsApp::build(). null on error. Handlers/scroll state persist on
    // the engine across frames.
    ui::UiNode* render(ui::NodeArena& arena);

    // Fire a handler (onPress id). Runs JS + drains microtasks. Returns true if
    // the app requested a re-render (setState happened).
    bool callHandler(int id);

    // True (and clears) if a re-render was requested since the last check.
    bool takeDirty();

    const std::string& lastError() const { return err_; }
    void setMemoryLimit(size_t bytes);
    void setMaxStackSize(size_t bytes);   // expose QuickJS recursion budget (experiments)
    void setDeadlineMs(uint32_t ms);

    void* context() const { return ctx_; }
    int   interruptCheck();          // internal (interrupt trampoline)
    void  markDirty() { dirty_ = true; }   // internal (schedule from JS)

    // Wire host services + install the capability-gated `nema` global (system
    // API: log/device/storage/http…). Call before loadApp(). (Plan 37 Fase 4.)
    void setHost(nema::Runtime* rt, std::string appId);
    nema::Runtime*    host()  const { return host_; }
    const std::string& appId() const { return appId_; }

    // Embedded-runtime module caching (used by the module loader).
    JSModuleDef* nemaModuleDef() const { return nemaDef_; }
    void         setNemaModuleDef(JSModuleDef* d) { nemaDef_ = d; }

    // Stable userdata for UiNode onPress thunks (id → JS handler).
    struct HandlerRef { JsEngine* eng; int id; };

private:
    void     captureError();
    void     pumpJobs();
    // Drain microtasks, then detect a rejected module-evaluation promise (an ES
    // module that throws at top level evaluates to a rejected promise, not a
    // synchronous exception). Frees `res`. Returns false + captures the error on
    // rejection. Without this, a module that throws at load would look "loaded".
    bool     settleEval(JSValue res);
    void     freeHandlers();
    void     installApi();          // build the `nema` global (js_api.cpp)
    ui::UiNode* reify(JSValueConst node, ui::NodeArena& arena);

    nema::Runtime* host_ = nullptr;
    std::string     appId_;

    JSRuntime* rt_  = nullptr;
    JSContext* ctx_ = nullptr;
    std::string err_;

    uint32_t deadlineMs_ = 0;
    uint64_t startMs_    = 0;
    bool     dirty_      = false;

    JSValue      appComponent_ = JS_UNDEFINED;   // app default export
    JSValue      renderFn_     = JS_UNDEFINED;   // nema.renderToTree
    JSValue      scheduleFn_   = JS_UNDEFINED;   // C fn passed to renderToTree
    JSModuleDef* nemaDef_     = nullptr;        // cached embedded-runtime module

    std::vector<JSValue>    handlers_;       // onPress/onChange fns (per frame)
    std::vector<HandlerRef> refs_;           // stable userdata for UiNode thunks

    // Persistent state across frames for JS <ScrollView>/<Slider>, keyed by
    // render-order index (reset each render).
    std::vector<ui::ScrollState*> scrolls_;
    std::vector<int>              sliderVals_;
    int scrollCursor_ = 0;
    int sliderCursor_ = 0;
};

} // namespace nema::js
