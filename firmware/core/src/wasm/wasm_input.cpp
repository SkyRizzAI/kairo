// Plan 86 Fase 4 — input.* host imports: raw input + timing for WASM apps.
//
// These imports serve raw-canvas apps that manage their own render loop and
// need to poll or wait for user input between frames. Retained-UI apps use
// ui_wait_event() instead (which integrates focus-nav internally).
//
// Action mapping (ABI constants → see nema_api.h):
//   ACT_NONE=0  ACT_PREV=1  ACT_NEXT=2  ACT_ACTIVATE=3  ACT_BACK=4
//   ACT_UP=5    ACT_DOWN=6
// Host maps input::Action → ABI int. Only Press events are returned;
// Release and Repeat are silently dropped so the guest sees clean edges.

#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "nema/input/input_action.h"
#include "nema/input_event.h"
#include "nema/thread.h"
#include "wasm3.h"
#include "m3_env.h"

namespace nema {
namespace {

static WasmHostCtx* hostOf(IM3Runtime rt) {
    return static_cast<WasmHostCtx*>(m3_GetUserData(rt));
}

// Map input::Action to the WASM ABI integer constant.
static int32_t actionToAbi(input::Action a) {
    switch (a) {
        case input::Action::Prev:        return 1;  // ACT_PREV
        case input::Action::Next:        return 2;  // ACT_NEXT
        case input::Action::Activate:    return 3;  // ACT_ACTIVATE
        case input::Action::Back:        return 4;  // ACT_BACK
        case input::Action::AdjustUp:    return 5;  // ACT_UP
        case input::Action::AdjustDown:  return 6;  // ACT_DOWN
        default:                         return 0;  // ACT_NONE
    }
}

// Drain one Press event from the mailbox; skip Release/Repeat/Pointer.
// Returns ACT_NONE if the mailbox is empty (non-blocking = poll semantics).
static int32_t pollOne(ISurface* surface) {
    if (!surface) return 0;
    InputEvent ev;
    while (surface->nextInput(ev)) {
        if (ev.kind == InputEvent::Kind::Key &&
            ev.type == InputEvent::Type::Press) {
            return actionToAbi(ev.action);
        }
    }
    return 0;
}

// ── input.input_poll() → i32 ──────────────────────────────────────────────

m3ApiRawFunction(wasm_input_poll) {
    m3ApiReturnType(int32_t);
    WasmHostCtx* h = hostOf(runtime);
    if (!h) m3ApiReturn(0);
    m3ApiReturn(pollOne(h->surface));
}

// ── input.input_wait(timeout_ms) → i32 ───────────────────────────────────

m3ApiRawFunction(wasm_input_wait) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, timeoutMs);
    WasmHostCtx* h = hostOf(runtime);
    if (!h || !h->surface) m3ApiReturn(0);

    // Try non-blocking first so we don't miss an event already queued.
    int32_t act = pollOne(h->surface);
    if (act != 0) m3ApiReturn(act);

    // Block up to timeoutMs. waitInput() returns false on timeout.
    InputEvent ev;
    uint32_t ms = (timeoutMs > 0) ? (uint32_t)timeoutMs : 0xFFFFFFFFu;
    while (true) {
        if (!h->surface->waitInput(ev, ms)) m3ApiReturn(0);  // timeout → NONE
        if (ev.kind == InputEvent::Kind::Key &&
            ev.type == InputEvent::Type::Press) {
            m3ApiReturn(actionToAbi(ev.action));
        }
        // Non-press event — keep waiting (re-enter with remaining budget).
        // We don't track sub-ms elapsed here; just re-try with same ms.
        // For the app, a spurious extra-wait on non-press events is fine.
    }
}

// ── input.delay(ms) ───────────────────────────────────────────────────────

m3ApiRawFunction(wasm_delay) {
    m3ApiGetArg(int32_t, ms);
    if (ms > 0) Thread::sleepMs((uint32_t)ms);
    m3ApiSuccess();
}

} // anon namespace

void linkInputImports(IM3Module mod) {
    auto link = [mod](const char* fn, const char* sig, M3RawCall cb) {
        m3_LinkRawFunction(mod, "input", fn, sig, cb);
    };
    link("input_poll", "i()",  &wasm_input_poll);
    link("input_wait", "i(i)", &wasm_input_wait);
    link("delay",      "v(i)", &wasm_delay);
}

} // namespace nema
