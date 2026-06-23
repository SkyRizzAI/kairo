// Plan 86 Fase 3 — ui.* host imports: retained-mode UI for WASM apps.
//
// Model (Invariant I7 — NO host→guest callbacks):
//   The guest owns its event loop. It calls ui_begin()..ui_end() to describe
//   one frame, then ui_wait_event() to block until the user activates a widget
//   or presses Back. The host runs focus-nav internally.
//
// Threading: all ui.* imports run on the app thread (ComponentApp::onStart
// blocks there). The GUI thread blits whatever was last present()-ed.
//
// First ui.* call → enterGuiMode(). Back in ui_wait_event() also calls
// ctx->requestExit(0) so ComponentApp::run() exits cleanly after WASM returns.

#include "nema/wasm/wasm_engine.h"
#include "nema/ui/widgets.h"
#include "nema/ui/component_runtime.h"
#include "nema/ui/text_style.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"
#include "nema/proc/process_context.h"
#include "nema/input_event.h"
#include "wasm3.h"
#include "m3_env.h"

namespace nema {
namespace {

// ── Per-run UI state ───────────────────────────────────────────────────────

// 256 nodes: header area (title + text) + ListContainer + list rows (up to ~40 APs).
static constexpr size_t UI_ARENA_CAP = 256;
static constexpr size_t UI_STR_CAP   = 2048;
static constexpr int    UI_STACK_MAX = 8;

struct UiStackFrame {
    aether::ui::UiNode* node;
    aether::ui::UiNode* lastChild;
};

struct UiState {
    aether::ui::NodeArena      arena;
    aether::ui::ComponentState cst;    // preserves focus across frames in one run
    aether::ui::ScrollState    scroll; // owned by the ListContainer
    int                        pendingId  = 0;
    aether::ui::UiNode*        root       = nullptr;
    aether::ui::UiNode*        header     = nullptr; // non-scrolling title/text area
    aether::ui::UiNode*        headerLast = nullptr;
    aether::ui::UiNode*        list       = nullptr; // ListContainer — scrollable buttons
    aether::ui::UiNode*        listLast   = nullptr;
    bool                       built      = false;

    UiStackFrame stack[UI_STACK_MAX];
    int          stackDepth = 0;

    // Skip the first Activate received — it's the launch-Select that opened the
    // app from the app list (same as WasmApp::ignoreKeys_ for JS/native apps).
    bool skipFirstActivate = true;

    char strPool[UI_STR_CAP];
    int  strUsed = 0;

    explicit UiState() : arena(UI_ARENA_CAP) {}

    void beginFrame() {
        arena.reset();
        strUsed    = 0;
        pendingId  = 0;
        root = header = headerLast = list = listLast = nullptr;
        built      = false;
        stackDepth = 0;
    }

    void fullReset() {
        beginFrame();
        cst               = {};
        scroll            = {};
        skipFirstActivate = true;
    }

    const char* intern(const char* s) {
        if (!s) return "";
        int len = 0; while (s[len]) len++;
        if (strUsed + len + 1 > (int)UI_STR_CAP) return "(…)";
        char* dst = strPool + strUsed;
        for (int i = 0; i <= len; i++) dst[i] = s[i];
        strUsed += len + 1;
        return dst;
    }

    // Add text/title to the non-scrolling header area at the top.
    void addToHeader(aether::ui::UiNode* node) {
        if (!node || !header) return;
        if (!headerLast) header->firstChild = node;
        else             headerLast->nextSibling = node;
        headerLast = node;
    }

    // Add a ListItemRow to the scrollable list. Creates the ListContainer on
    // the first call and appends it to root after the header.
    void addToList(aether::ui::UiNode* node) {
        if (!node) return;
        if (!list) {
            list = aether::ui::ListContainer(arena, scroll, {});
            if (!list) return;
            // Link after header (or as first child if no header).
            if (header) header->nextSibling = list;
            else if (!root->firstChild) root->firstChild = list;
        }
        if (!listLast) list->firstChild = node;
        else           listLast->nextSibling = node;
        listLast = node;
    }

    // Append a child to the current top-of-stack container (for row/col begin).
    void addChild(aether::ui::UiNode* child) {
        if (!child || stackDepth == 0) return;
        UiStackFrame& top = stack[stackDepth - 1];
        if (!top.node->firstChild)        top.node->firstChild = child;
        else if (top.lastChild)           top.lastChild->nextSibling = child;
        top.lastChild = child;
    }

    void pushContainer(aether::ui::UiNode* node) {
        if (!node || stackDepth >= UI_STACK_MAX) return;
        addChild(node);
        stack[stackDepth++] = {node, nullptr};
    }
};

// One active UiState per app-thread WASM run (single-threaded).
// Allocated once; reset between runs via fullReset().
static UiState& uiStateStorage() {
    static UiState s;
    return s;
}
static UiState* gUiState = nullptr;

// Button callback: called synchronously by dispatchNav(Activate).
static void onButtonPress(void* userdata) {
    if (gUiState) gUiState->pendingId = (int)(intptr_t)userdata;
}

// ── Helpers ────────────────────────────────────────────────────────────────

static WasmHostCtx* hostOf(IM3Runtime rt) {
    return static_cast<WasmHostCtx*>(m3_GetUserData(rt));
}

static bool readCStr(IM3Runtime rt, uint32_t off, const char*& out) {
    uint32_t sz = 0;
    uint8_t* base = m3_GetMemory(rt, &sz, 0);
    if (!base || off >= sz) return false;
    uint32_t end = off;
    while (end < sz && base[end]) end++;
    if (end >= sz) return false;
    out = reinterpret_cast<const char*>(base + off);
    return true;
}

static void renderFrame(WasmHostCtx* h, UiState* st) {
    if (!h->surface || !st->root) return;
    Canvas& c = h->surface->canvas();
    c.clear(false);
    // Start below the system status bar, same origin as component_screen.cpp.
    int16_t  oy = (int16_t)nema::display::contentY();
    uint16_t ah = (uint16_t)(c.height() - oy);
    aether::ui::renderComponentFrame(
        st->root, c, st->cst, aether::ui::roleMetrics(),
        0, oy, c.width(), ah);
    h->surface->present();
}

// ── ui.* imports ──────────────────────────────────────────────────────────

m3ApiRawFunction(wasm_ui_begin) {
    WasmHostCtx* h = hostOf(runtime);
    if (!h) m3ApiSuccess();
    if (h->surface) h->surface->enterGuiMode();

    // First call in a new run: full reset. Subsequent calls: per-frame only.
    if (!gUiState) {
        gUiState = &uiStateStorage();
        gUiState->fullReset();
    } else {
        gUiState->beginFrame();
    }
    UiState& st = *gUiState;

    // Root: full-screen Col — header (title/text) on top, ListContainer below.
    aether::ui::Style rootSt;
    rootSt.dir      = aether::ui::FlexDir::Col;
    rootSt.flexGrow = 1;
    rootSt.align    = aether::ui::Align::Stretch;   // children fill full width
    st.root = aether::ui::View(st.arena, rootSt);
    if (!st.root) m3ApiSuccess();

    // Header sub-view: non-scrolling title / description area.
    aether::ui::Style hdrSt;
    hdrSt.dir     = aether::ui::FlexDir::Col;
    hdrSt.padding = aether::theme().space.sm;
    hdrSt.gap     = 2;
    st.header = aether::ui::View(st.arena, hdrSt);
    if (st.header) st.root->firstChild = st.header;

    // Stack kept for ui_row_begin / ui_col_begin compatibility.
    st.stack[0]   = {st.root, nullptr};
    st.stackDepth = 1;
    m3ApiSuccess();
}

m3ApiRawFunction(wasm_ui_title) {
    m3ApiGetArg(uint32_t, msgOff);
    WasmHostCtx* h = hostOf(runtime);
    UiState* st = gUiState;
    if (!h || !st) m3ApiSuccess();
    const char* raw; if (!readCStr(runtime, msgOff, raw)) m3ApiSuccess();
    // Subhead: compact bold section header (matches ListView section style).
    st->addToHeader(aether::ui::Text(st->arena, st->intern(raw),
                                     aether::ui::TextRole::Subhead));
    m3ApiSuccess();
}

m3ApiRawFunction(wasm_ui_text) {
    m3ApiGetArg(uint32_t, msgOff);
    WasmHostCtx* h = hostOf(runtime);
    UiState* st = gUiState;
    if (!h || !st) m3ApiSuccess();
    const char* raw; if (!readCStr(runtime, msgOff, raw)) m3ApiSuccess();
    st->addToHeader(aether::ui::Text(st->arena, st->intern(raw),
                                     aether::ui::TextRole::Body));
    m3ApiSuccess();
}

m3ApiRawFunction(wasm_ui_button) {
    m3ApiGetArg(uint32_t, labelOff);
    m3ApiGetArg(int32_t,  id);
    WasmHostCtx* h = hostOf(runtime);
    UiState* st = gUiState;
    if (!h || !st || id <= 0) m3ApiSuccess();
    const char* raw; if (!readCStr(runtime, labelOff, raw)) m3ApiSuccess();
    // Render as a ListItemRow (same style as Settings / Files browser).
    aether::ui::ListEntry e;
    e.label   = st->intern(raw);
    e.chevron = false;
    e.onPress = &onButtonPress;
    e.user    = (void*)(intptr_t)id;
    st->addToList(aether::ui::ListItemRow(st->arena, e));
    m3ApiSuccess();
}

m3ApiRawFunction(wasm_ui_row_begin) {
    UiState* st = gUiState;
    if (!st) m3ApiSuccess();
    aether::ui::Style s; s.dir = aether::ui::FlexDir::Row;
    st->pushContainer(aether::ui::View(st->arena, s));
    m3ApiSuccess();
}

m3ApiRawFunction(wasm_ui_row_end) {
    UiState* st = gUiState;
    if (st && st->stackDepth > 1) st->stackDepth--;
    m3ApiSuccess();
}

m3ApiRawFunction(wasm_ui_col_begin) {
    UiState* st = gUiState;
    if (!st) m3ApiSuccess();
    aether::ui::Style s; s.dir = aether::ui::FlexDir::Col;
    st->pushContainer(aether::ui::View(st->arena, s));
    m3ApiSuccess();
}

m3ApiRawFunction(wasm_ui_col_end) {
    UiState* st = gUiState;
    if (st && st->stackDepth > 1) st->stackDepth--;
    m3ApiSuccess();
}

m3ApiRawFunction(wasm_ui_end) {
    WasmHostCtx* h = hostOf(runtime);
    UiState* st = gUiState;
    if (!h || !st || !st->root) m3ApiSuccess();
    st->stackDepth = 0;  // seal: any unclosed begin() collapses to root
    st->built = true;
    renderFrame(h, st);
    m3ApiSuccess();
}

static constexpr int32_t EV_BACK = -1;
static constexpr int32_t EV_NONE =  0;

// Translate Key → Nav. Returns false if key should not drive focus.
static bool keyToNav(Key k, aether::ui::Nav& out) {
    switch (k) {
        case Key::Up:     out = aether::ui::Nav::Prev;     return true;
        case Key::Down:   out = aether::ui::Nav::Next;     return true;
        case Key::Select: out = aether::ui::Nav::Activate; return true;
        case Key::Left:   out = aether::ui::Nav::Prev;     return true;
        case Key::Right:  out = aether::ui::Nav::Next;     return true;
        default:          return false;
    }
}

m3ApiRawFunction(wasm_ui_wait_event) {
    m3ApiReturnType(int32_t);
    WasmHostCtx* h = hostOf(runtime);
    UiState* st = gUiState;
    if (!h || !st || !st->built) m3ApiReturn(EV_BACK);

    while (true) {
        // If requestExit() was already called (e.g. by Back on a parent screen),
        // waitInput() returns false immediately and we'd spin. Exit cleanly.
        if (h->ctx->shouldExit()) m3ApiReturn(EV_BACK);

        InputEvent ev;
        if (!h->surface->waitInput(ev, 30000)) continue;  // timeout keepalive

        if (ev.kind != InputEvent::Kind::Key) continue;
        if (ev.type != InputEvent::Type::Press) continue;

        if (ev.key == Key::Cancel) {
            // Signal ComponentApp::run() to exit cleanly after WASM returns.
            h->ctx->requestExit(0);
            m3ApiReturn(EV_BACK);
        }

        st->pendingId = 0;
        aether::ui::Nav nav;
        if (keyToNav(ev.key, nav)) {
            aether::ui::dispatchNav(st->root, st->cst, nav);
        }
        if (st->pendingId != 0) {
            // Discard the very first Activate — it's the launch-Select that
            // opened the app from the launcher (matches WasmApp::ignoreKeys_).
            if (st->skipFirstActivate) {
                st->skipFirstActivate = false;
                st->pendingId = 0;
                renderFrame(h, st);
                continue;
            }
            m3ApiReturn((int32_t)st->pendingId);
        }

        // Focus moved (or no-op) — redraw with new highlight.
        renderFrame(h, st);
    }
}

m3ApiRawFunction(wasm_ui_poll_event) {
    m3ApiReturnType(int32_t);
    WasmHostCtx* h = hostOf(runtime);
    UiState* st = gUiState;
    if (!h || !st || !st->built) m3ApiReturn(EV_NONE);

    InputEvent ev;
    if (!h->surface->nextInput(ev)) m3ApiReturn(EV_NONE);

    if (ev.kind == InputEvent::Kind::Key && ev.type == InputEvent::Type::Press) {
        if (ev.key == Key::Cancel) {
            h->ctx->requestExit(0);
            m3ApiReturn(EV_BACK);
        }
        st->pendingId = 0;
        aether::ui::Nav nav;
        if (keyToNav(ev.key, nav)) {
            aether::ui::dispatchNav(st->root, st->cst, nav);
        }
        if (st->pendingId != 0) m3ApiReturn((int32_t)st->pendingId);
        renderFrame(h, st);
    }
    m3ApiReturn(EV_NONE);
}

} // anon namespace

// Called by wasm_engine.cpp to clean up between runs.
void resetUiState() { gUiState = nullptr; }

void linkUiImports(IM3Module mod) {
    auto link = [mod](const char* fn, const char* sig, M3RawCall cb) {
        m3_LinkRawFunction(mod, "ui", fn, sig, cb);
    };
    link("ui_begin",      "v()",   &wasm_ui_begin);
    link("ui_title",      "v(*)",  &wasm_ui_title);
    link("ui_text",       "v(*)",  &wasm_ui_text);
    link("ui_button",     "v(*i)", &wasm_ui_button);
    link("ui_row_begin",  "v()",   &wasm_ui_row_begin);
    link("ui_row_end",    "v()",   &wasm_ui_row_end);
    link("ui_col_begin",  "v()",   &wasm_ui_col_begin);
    link("ui_col_end",    "v()",   &wasm_ui_col_end);
    link("ui_end",        "v()",   &wasm_ui_end);
    link("ui_wait_event", "i()",   &wasm_ui_wait_event);
    link("ui_poll_event", "i()",   &wasm_ui_poll_event);
}

} // namespace nema
