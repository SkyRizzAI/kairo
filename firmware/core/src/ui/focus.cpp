#include "nema/ui/focus.h"

namespace aether::ui {
using namespace nema;

// Single DFS pass: count all focusable nodes; capture node at `target` index.
// Returns total focusable count in the subtree.
static int countAndFind(UiNode* n, int target, UiNode** found, int idx) {
    if (!n) return idx;
    if (n->focusable) {
        if (idx == target) *found = n;
        idx++;
    }
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        idx = countAndFind(k, target, found, idx);
    return idx;
}

UiNode* focusedNode(UiNode& root, FocusState& fs) {
    UiNode* found = nullptr;
    int n = countAndFind(&root, fs.focused, &found, 0);
    fs.count = n;
    if (n == 0) { fs.focused = 0; return nullptr; }
    if (fs.focused < 0)  fs.focused = 0;
    if (fs.focused >= n) fs.focused = n - 1;
    // Clamp may have changed the index — re-find if needed.
    if (!found) {
        found = nullptr;
        countAndFind(&root, fs.focused, &found, 0);
    }
    return found;
}

bool handleFocusKey(UiNode& root, FocusState& fs, Key k) {
    UiNode* found = nullptr;
    int n = countAndFind(&root, fs.focused, &found, 0);
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
            if (!found) {
                found = nullptr;
                countAndFind(&root, fs.focused, &found, 0);
            }
            if (found && found->onPress) { found->onPress(found->userdata); return true; }
            return false;
        }
        default:
            return false;
    }
}

} // namespace aether::ui
