// Repro: embedded apps import from BOTH "nema" and "nema/jsx-dev-runtime".
// The host js_render_test only imports from "nema", so it never exercises the
// dual-name → same-JSModuleDef path. This test reproduces the real apps.
#include "nema/js/js_engine.h"
#include "nema/apps/embedded_apps.h"
#include <cstdio>

using namespace nema;

int main() {
    std::printf("== JS dual-import (real embedded apps) ==\n");
    int fail = 0;

    // Minimal dual-import app, like the generated ones.
    static const char* DUAL =
        "import { View, Text } from 'nema';\n"
        "import { jsxDEV } from 'nema/jsx-dev-runtime';\n"
        "export default function App() {\n"
        "  return jsxDEV(View, { children: jsxDEV(Text, { children: 'hi' }) });\n"
        "}\n";

    {
        js::JsEngine eng;
        bool loaded = eng.loadApp(DUAL, "dual.min");
        std::printf("  minimal dual-import loaded=%d err=%s\n",
                    loaded, eng.lastError().c_str());
        if (!loaded) fail++;
    }

    // Every built-in app must LOAD (this is the regression: it used to fail on
    // simulator/device due to the uncoordinated stack guard). Rendering is not
    // asserted here — some apps use the `nema` system global, which needs a live
    // Runtime (see js_render_test for the render path).
    for (int i = 0; i < EMBEDDED_APP_COUNT; i++) {
        js::JsEngine eng;
        bool loaded = eng.loadApp(EMBEDDED_APPS[i].js, EMBEDDED_APPS[i].id);
        std::printf("  [%s] loaded=%d err=%s\n",
                    EMBEDDED_APPS[i].id, loaded, eng.lastError().c_str());
        if (!loaded) fail++;
    }

    std::printf(fail == 0 ? "== ALL PASS ==\n" : "== FAILURES ==\n");
    return fail ? 1 : 0;
}
