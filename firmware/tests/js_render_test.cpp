// Host test for the JS→UiNode bridge (Plan 37 Fase 2): load a JS app module,
// render its component to a native UiNode tree, and fire an onPress back into JS.
#include "kairo/js/js_engine.h"
#include "kairo/ui/node.h"
#include "kairo/ui/widgets.h"
#include <cstdio>
#include <cstring>

using namespace kairo;
using namespace kairo::ui;

static int fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); fail++; } \
                         else std::printf("  ok:   %s\n", m); } while (0)

// A reactive counter authored against the `kairo` runtime using jsx() directly
// (the TSX→jsx transform happens at build time; here we hand-write the calls).
// Exercises both the bridge (Fase 2) and hooks/re-render (Fase 3).
static const char* APP =
    "import { View, Text, Pressable, useState, jsx } from 'kairo';\n"
    "export default function App() {\n"
    "  const [n, setN] = useState(0);\n"
    "  return jsx(View, { style:{flexDirection:'column',padding:4}, children: [\n"
    "    jsx(Text, { variant:'title', children: 'Count: ' + n }),\n"
    "    jsx(Pressable, { onPress: () => setN(n + 1),\n"
    "                     children: jsx(Text, { children: '+' }) }),\n"
    "  ]});\n"
    "}\n";

static UiNode* titleOf(UiNode* root) { return root ? root->firstChild : nullptr; }
static UiNode* pressOf(UiNode* root) { auto* t = titleOf(root); return t ? t->nextSibling : nullptr; }

int main() {
    std::printf("== JS→UiNode bridge tests ==\n");
    js::JsEngine eng;
    CHECK(eng.ok(), "engine init");
    bool loaded = eng.loadApp(APP);
    if (!loaded) std::printf("  loadApp error: %s\n", eng.lastError().c_str());
    CHECK(loaded, "loadApp (resolves `kairo`, gets default export)");

    NodeArena arena(256);
    UiNode* root = eng.render(arena);
    if (!root) std::printf("  render error: %s\n", eng.lastError().c_str());
    CHECK(root && root->type == NodeType::View, "root reifies to View");
    CHECK(root && root->style.dir == FlexDir::Col, "style flexDirection:column applied");

    UiNode* title = titleOf(root);
    CHECK(title && title->type == NodeType::Text && title->role == TextRole::Title,
          "first child = Text(title)");
    CHECK(title && title->text && std::strcmp(title->text, "Count: 0") == 0,
          "title text folded = 'Count: 0'");

    UiNode* press = pressOf(root);
    CHECK(press && press->type == NodeType::Pressable && press->focusable,
          "second child = focusable Pressable");
    CHECK(press && press->onPress != nullptr, "Pressable has onPress thunk");

    // Fase 3: fire onPress (setN(1)) → re-render → counter reflects new state.
    if (press && press->onPress) press->onPress(press->userdata);
    UiNode* root2 = eng.render(arena);   // hooks state persists across renders
    UiNode* title2 = titleOf(root2);
    CHECK(title2 && title2->text && std::strcmp(title2->text, "Count: 1") == 0,
          "after onPress → re-render shows 'Count: 1' (useState reactivity)");

    // Once more → 'Count: 2'.
    UiNode* press2 = pressOf(root2);
    if (press2 && press2->onPress) press2->onPress(press2->userdata);
    UiNode* title3 = titleOf(eng.render(arena));
    CHECK(title3 && title3->text && std::strcmp(title3->text, "Count: 2") == 0,
          "second onPress → 'Count: 2'");

    std::printf(fail == 0 ? "== ALL PASS ==\n" : "== FAILURES ==\n");
    return fail == 0 ? 0 : 1;
}
