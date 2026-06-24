// Plan 90 F1.3 — Aether UI debug tools implementation.
#include "nema/ui/aether_debug.h"
#include "nema/log/logger.h"
#include <cstdio>

namespace aether::ui::debug {
using namespace nema;

static const char* nodeTypeName(NodeType t) {
    switch (t) {
        case NodeType::View:          return "View";
        case NodeType::Text:          return "Text";
        case NodeType::Pressable:     return "Pressable";
        case NodeType::Scroll:        return "Scroll";
        case NodeType::Slider:        return "Slider";
        case NodeType::Icon:          return "Icon";
        case NodeType::AnimatedIcon:  return "AnimIcon";
        default:                      return "?";
    }
}

void dumpTree(const UiNode* root, Logger& log, int depth) {
    if (!root) return;
    char indent[33] = {};
    for (int i = 0; i < depth * 2 && i < 32; i++) indent[i] = ' ';

    char buf[128];
    snprintf(buf, sizeof(buf), "%s%s (%d,%d) %dx%d%s%s",
        indent,
        nodeTypeName(root->type),
        root->x, root->y, root->w, root->h,
        root->text ? " t=" : "",
        root->text ? root->text : "");
    log.debug("UI", buf);

    for (const UiNode* k = root->firstChild; k; k = k->nextSibling)
        dumpTree(k, log, depth + 1);
}

void dumpStats(const NodeArena& arena, const ComponentState& st, Logger& log) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "nodes=%zu/%zu focus=%d/%d overflow=%s",
        arena.used(), arena.capacity(),
        st.focus.focused, st.focus.count,
        arena.overflowed() ? "YES" : "no");
    log.debug("UI", buf);
}

} // namespace aether::ui::debug
