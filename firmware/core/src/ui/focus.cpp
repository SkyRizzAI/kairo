#include "kairo/ui/focus.h"

namespace kairo::ui {

// DFS collect focusable nodes in tree order. Returns count; fills out[] up to max.
static int collect(UiNode* n, UiNode** out, int max, int idx) {
    if (!n) return idx;
    if (n->focusable && n->onPress != nullptr) {
        if (idx < max) out[idx] = n;
        idx++;
    }
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        idx = collect(k, out, max, idx);
    return idx;
}

static constexpr int MAX_FOCUS = 64;

UiNode* focusedNode(UiNode& root, FocusState& fs) {
    UiNode* list[MAX_FOCUS];
    int n = collect(&root, list, MAX_FOCUS, 0);
    fs.count = n;
    if (n == 0) { fs.focused = 0; return nullptr; }
    if (fs.focused < 0)  fs.focused = 0;
    if (fs.focused >= n) fs.focused = n - 1;
    return list[fs.focused];
}

bool handleFocusKey(UiNode& root, FocusState& fs, Key k) {
    UiNode* list[MAX_FOCUS];
    int n = collect(&root, list, MAX_FOCUS, 0);
    fs.count = n;
    if (n == 0) return false;
    if (fs.focused < 0)  fs.focused = 0;
    if (fs.focused >= n) fs.focused = n - 1;

    switch (k) {
        case Key::Up:
        case Key::Left:
            fs.focused = (fs.focused - 1 + n) % n;
            return true;
        case Key::Down:
        case Key::Right:
            fs.focused = (fs.focused + 1) % n;
            return true;
        case Key::Select: {
            UiNode* f = list[fs.focused];
            if (f->onPress) { f->onPress(f->userdata); return true; }
            return false;
        }
        default:
            return false;
    }
}

} // namespace kairo::ui
