// Proves the "an app may fail, but the OS must not" contract at the engine level:
// a too-deep / runaway script throws a CLEAN, catchable error (captured, engine
// survives) instead of corrupting the stack. Covers both deep *parsing* (the ESP
// freeze risk) and deep *execution*, under a coordinated stack guard.
#include "nema/js/js_engine.h"
#include "nema/apps/embedded_apps.h"
#include <cstdio>
#include <string>

using namespace nema;

static int fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); fail++; } \
                         else std::printf("  ok:   %s\n", m); } while (0)

int main() {
    std::printf("== JS graceful-failure (OS must survive) ==\n");

    // Model a constrained device: a modest guard well below the host stack.
    const size_t GUARD = 96 * 1024;

    // 1) Pathologically deep *parse* (nested arrays). Must throw cleanly, not crash.
    {
        std::string deep = "export default function App(){ return ";
        for (int i = 0; i < 20000; i++) deep += "[";
        deep += "0";
        for (int i = 0; i < 20000; i++) deep += "]";
        deep += "; }\n";
        js::JsEngine eng;
        eng.setMaxStackSize(GUARD);
        bool ok = eng.loadApp(deep.c_str(), "deep.parse");
        CHECK(!ok, "deep-nested parse rejected (not crashed)");
        CHECK(!eng.lastError().empty(), "  error captured for deep parse");
        std::printf("    err: %.60s\n", eng.lastError().c_str());
    }

    // 2) Infinite *recursion* at module top-level. Must throw cleanly.
    {
        js::JsEngine eng;
        eng.setMaxStackSize(GUARD);
        bool ok = eng.loadApp("function f(){return f()} f();\n"
                              "export default function App(){ return null; }\n", "deep.exec");
        CHECK(!ok, "infinite recursion rejected (not crashed)");
        CHECK(eng.lastError().find("stack") != std::string::npos ||
              eng.lastError().find("call stack") != std::string::npos,
              "  error mentions stack overflow");
        std::printf("    err: %.60s\n", eng.lastError().c_str());
    }

    // 3) After failures the process is fine: a real app loads under the SAME guard.
    {
        js::JsEngine eng;
        eng.setMaxStackSize(GUARD);
        bool ok = eng.loadApp(EMBEDDED_APPS[0].js, EMBEDDED_APPS[0].id);
        CHECK(ok, "real app still loads under the coordinated guard");
        if (!ok) std::printf("    err: %s\n", eng.lastError().c_str());
    }

    std::printf(fail == 0 ? "== ALL PASS ==\n" : "== FAILURES ==\n");
    return fail ? 1 : 0;
}
