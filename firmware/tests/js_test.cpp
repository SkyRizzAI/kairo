// Host test for the embedded QuickJS engine wrapper (Plan 37 Fase 1).
#include "nema/js/js_engine.h"
#include <cstdio>

using namespace nema::js;

static int fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); fail++; } \
                         else std::printf("  ok:   %s\n", m); } while (0)

int main() {
    std::printf("== JsEngine (QuickJS) tests ==\n");

    JsEngine e;
    CHECK(e.ok(), "engine init");
    CHECK(e.eval("1 + 1"), "eval 1+1");
    CHECK(e.eval("globalThis.__r = JSON.stringify([1,2,3].map(n => n*2));"
                 "if (__r !== '[2,4,6]') throw new Error('bad: ' + __r)"),
          "ES2020 (arrow + map + JSON + template) works");
    CHECK(e.eval("(async () => 42)().then(v => { if (v!==42) throw new Error('p') })"),
          "Promise/async pending-job drain");

    CHECK(!e.eval("this is :: not js"), "syntax error → false");
    CHECK(!e.lastError().empty(), "error message captured");

    // Runaway-app guard: deadline interrupt.
    e.setDeadlineMs(200);
    CHECK(!e.eval("while (true) {}"), "infinite loop interrupted by deadline");
    CHECK(e.eval("2 + 2"), "engine still usable after interrupt");

    std::printf(fail == 0 ? "== ALL PASS ==\n" : "== FAILURES ==\n");
    return fail == 0 ? 0 : 1;
}
