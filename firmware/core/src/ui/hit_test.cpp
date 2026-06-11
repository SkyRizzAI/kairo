#include "kairo/ui/hit_test.h"

namespace kairo::ui {

static UiNode* hit(UiNode* n, int16_t x, int16_t y) {
    UiNode* found = nullptr;
    if (nodeContains(n, x, y) && n->focusable && (n->onPress || n->onAdjust)) found = n;
    // Children paint over parent → a child match takes precedence (top-most).
    for (UiNode* k = n->firstChild; k; k = k->nextSibling) {
        if (UiNode* c = hit(k, x, y)) found = c;
    }
    return found;
}

UiNode* hitTest(UiNode& root, int16_t x, int16_t y) {
    return hit(&root, x, y);
}

} // namespace kairo::ui
