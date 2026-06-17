// Plan 50 — Aether ABI implementation.
// Thin shim over nema::ui widgets. Each function delegates to the existing
// widget builder (widgets.h). A thread-local NodeArena holds the current
// frame's tree; Plan 55 will replace this with per-surface arenas.

#include "nema/ui/aether_abi.h"
#include "nema/ui/widgets.h"
#include <cstring>

using namespace nema::ui;

// ── Per-frame arena ───────────────────────────────────────────────────────

static thread_local NodeArena* g_arena = nullptr;

void aether_set_arena(NodeArena* a) { g_arena = a; }
NodeArena* aether_get_arena() { return g_arena; }

// ── View stack (for begin/end nesting) ────────────────────────────────────

static constexpr int MAX_DEPTH = 16;
static thread_local UiNode* g_stack[MAX_DEPTH];
static thread_local int     g_depth = 0;

static UiNode* current_parent() {
    return g_depth > 0 ? g_stack[g_depth - 1] : nullptr;
}

// ── view interface ─────────────────────────────────────────────────────────

AetherView aether_view_begin(const char* direction) {
    if (!g_arena || g_depth >= MAX_DEPTH) return nullptr;
    Style st{};
    st.dir = (direction && std::strcmp(direction, "row") == 0) ? FlexDir::Row : FlexDir::Col;
    UiNode* node = View(*g_arena, st);
    if (!node) return nullptr;
    g_stack[g_depth++] = node;
    return node;
}

void aether_view_end(void) {
    if (g_depth > 0) g_depth--;
}

// ── text interface ─────────────────────────────────────────────────────────

static TextRole roleFromVariant(const char* variant) {
    if (!variant) return TextRole::Body;
    if (std::strcmp(variant, "title")   == 0) return TextRole::Title;
    if (std::strcmp(variant, "caption") == 0) return TextRole::Caption;
    return TextRole::Body;  // "body", "subtitle", etc → Body
}

AetherView aether_text_label(const char* content) {
    if (!g_arena) return nullptr;
    UiNode* node = Text(*g_arena, content ? content : "", TextRole::Body);
    if (node && current_parent()) setChildren(current_parent(), {node});
    return node;
}

AetherView aether_text_styled(const char* content, const char* variant) {
    if (!g_arena) return nullptr;
    UiNode* node = Text(*g_arena, content ? content : "", roleFromVariant(variant));
    if (node && current_parent()) setChildren(current_parent(), {node});
    return node;
}

// ── interactive interface ──────────────────────────────────────────────────

AetherView aether_interactive_button(const char* label, int32_t on_press) {
    if (!g_arena) return nullptr;
    UiNode* node = Button(*g_arena, label ? label : "", nullptr, nullptr);
    (void)on_press;  // callback wiring in Phase 3 (loader)
    if (node && current_parent()) setChildren(current_parent(), {node});
    return node;
}

// ── scroll interface ───────────────────────────────────────────────────────

static thread_local ScrollState g_scrollState;

AetherView aether_scroll_begin(void) {
    if (!g_arena || g_depth >= MAX_DEPTH) return nullptr;
    Style st{};
    st.dir = FlexDir::Col;
    UiNode* node = ScrollView(*g_arena, g_scrollState, st);
    if (!node) return nullptr;
    g_stack[g_depth++] = node;
    return node;
}

void aether_scroll_end(void) {
    if (g_depth > 0) g_depth--;
}
