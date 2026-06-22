// Plan 86 Fase 2 — canvas.* host imports for raw-pixel WASM apps.
// Maps the WASM canvas ABI (§5.1) to the host Canvas and ISurface.
//
// First call to any canvas.* import flips the host from Terminal to Gui mode
// (ISurface::enterGuiMode()) — irreversible for this run. canvas_flush() maps
// to ISurface::present(), which publishes the frame to the GUI thread.
//
// Color convention (Invariant I6): bool on — COLOR_BLACK=0→false, COLOR_WHITE=1→true.
// Guest passes int32_t color; host reads (color != 0) as bool.
//
// Coordinates are LOGICAL pixels (Canvas handles scale internally). WASM apps
// must use canvas_width()/canvas_height(), never hardcode dimensions (I2).

#include "nema/wasm/wasm_engine.h"
#include "nema/ui/canvas.h"
#include "wasm3.h"
#include "m3_env.h"
#include <cstring>

namespace nema {
namespace {

static WasmHostCtx* hostOf(IM3Runtime rt) {
    return static_cast<WasmHostCtx*>(m3_GetUserData(rt));
}

// Resolve a guest pointer to a host C-string. Returns false if out of bounds
// or unterminated.
static bool readCStr(IM3Runtime rt, uint32_t off, const char*& out) {
    uint32_t memSize = 0;
    uint8_t* base = m3_GetMemory(rt, &memSize, 0);
    if (!base || off >= memSize) return false;
    uint32_t end = off;
    while (end < memSize && base[end] != 0) end++;
    if (end >= memSize) return false;
    out = reinterpret_cast<const char*>(base + off);
    return true;
}

// Lazy-flip to Gui mode on first canvas call.
static inline void ensureGuiMode(WasmHostCtx* h) {
    if (h->surface) h->surface->enterGuiMode();
}

// Get the canvas (or return nullptr if no surface).
static inline Canvas* getCanvas(WasmHostCtx* h) {
    return h->surface ? &h->surface->canvas() : nullptr;
}

// ── canvas.canvas_width() → i32 ───────────────────────────────────────────

m3ApiRawFunction(canvas_width) {
    m3ApiReturnType(int32_t);
    WasmHostCtx* h = hostOf(runtime);
    if (!h || !h->surface) m3ApiReturn(0);
    m3ApiReturn((int32_t)h->surface->canvas().width());
}

// ── canvas.canvas_height() → i32 ──────────────────────────────────────────

m3ApiRawFunction(canvas_height) {
    m3ApiReturnType(int32_t);
    WasmHostCtx* h = hostOf(runtime);
    if (!h || !h->surface) m3ApiReturn(0);
    m3ApiReturn((int32_t)h->surface->canvas().height());
}

// ── canvas.canvas_clear(color) ────────────────────────────────────────────

m3ApiRawFunction(canvas_clear) {
    m3ApiGetArg(int32_t, color);
    WasmHostCtx* h = hostOf(runtime);
    if (!h) m3ApiSuccess();
    ensureGuiMode(h);
    Canvas* c = getCanvas(h);
    if (c) c->clear(color != 0);
    m3ApiSuccess();
}

// ── canvas.canvas_pixel(x, y, color) ──────────────────────────────────────

m3ApiRawFunction(canvas_pixel) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, color);
    WasmHostCtx* h = hostOf(runtime);
    if (!h) m3ApiSuccess();
    ensureGuiMode(h);
    Canvas* c = getCanvas(h);
    if (c && x >= 0 && y >= 0)
        c->drawPixel((uint16_t)x, (uint16_t)y, color != 0);
    m3ApiSuccess();
}

// ── canvas.canvas_fill_rect(x, y, w, h, color) ────────────────────────────

m3ApiRawFunction(canvas_fill_rect) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, w);
    m3ApiGetArg(int32_t, h);
    m3ApiGetArg(int32_t, color);
    WasmHostCtx* host = hostOf(runtime);
    if (!host) m3ApiSuccess();
    ensureGuiMode(host);
    Canvas* c = getCanvas(host);
    if (c && x >= 0 && y >= 0 && w > 0 && h > 0)
        c->fillRect((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, color != 0);
    m3ApiSuccess();
}

// ── canvas.canvas_rect(x, y, w, h, color) — outline ──────────────────────

m3ApiRawFunction(canvas_rect) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, w);
    m3ApiGetArg(int32_t, h);
    m3ApiGetArg(int32_t, color);
    WasmHostCtx* host = hostOf(runtime);
    if (!host) m3ApiSuccess();
    ensureGuiMode(host);
    Canvas* c = getCanvas(host);
    if (c && x >= 0 && y >= 0 && w > 0 && h > 0)
        c->drawRect((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, color != 0);
    m3ApiSuccess();
}

// ── canvas.canvas_line(x0, y0, x1, y1, color) ────────────────────────────

m3ApiRawFunction(canvas_line) {
    m3ApiGetArg(int32_t, x0);
    m3ApiGetArg(int32_t, y0);
    m3ApiGetArg(int32_t, x1);
    m3ApiGetArg(int32_t, y1);
    m3ApiGetArg(int32_t, color);
    WasmHostCtx* host = hostOf(runtime);
    if (!host) m3ApiSuccess();
    ensureGuiMode(host);
    Canvas* c = getCanvas(host);
    if (c && x0 >= 0 && y0 >= 0 && x1 >= 0 && y1 >= 0)
        c->drawLine((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1, color != 0);
    m3ApiSuccess();
}

// ── canvas.canvas_text(x, y, msg_ptr, color) ──────────────────────────────

m3ApiRawFunction(canvas_text) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(uint32_t, msgOff);
    m3ApiGetArg(int32_t, color);
    WasmHostCtx* host = hostOf(runtime);
    if (!host) m3ApiSuccess();
    ensureGuiMode(host);
    Canvas* c = getCanvas(host);
    if (!c || x < 0 || y < 0) m3ApiSuccess();
    const char* msg;
    if (!readCStr(runtime, msgOff, msg)) m3ApiSuccess();
    c->drawText((uint16_t)x, (uint16_t)y, msg, color != 0);
    m3ApiSuccess();
}

// ── canvas.canvas_flush() — present + ensure Gui mode ─────────────────────

m3ApiRawFunction(canvas_flush) {
    WasmHostCtx* h = hostOf(runtime);
    if (!h) m3ApiSuccess();
    ensureGuiMode(h);   // flip on first flush too (belt-and-suspenders)
    if (h->surface) h->surface->present();
    m3ApiSuccess();
}

} // anon namespace

// ── Public API ─────────────────────────────────────────────────────────────

void linkCanvasImports(IM3Module mod) {
    auto link = [mod](const char* fn, const char* sig, M3RawCall cb) {
        m3_LinkRawFunction(mod, "canvas", fn, sig, cb);
    };

    link("canvas_width",     "i()",       &canvas_width);
    link("canvas_height",    "i()",       &canvas_height);
    link("canvas_clear",     "v(i)",      &canvas_clear);
    link("canvas_pixel",     "v(iii)",    &canvas_pixel);
    link("canvas_fill_rect", "v(iiiii)",  &canvas_fill_rect);
    link("canvas_rect",      "v(iiiii)",  &canvas_rect);
    link("canvas_line",      "v(iiiii)",  &canvas_line);
    link("canvas_text",      "v(ii*i)",   &canvas_text);
    link("canvas_flush",     "v()",       &canvas_flush);
}

} // namespace nema
