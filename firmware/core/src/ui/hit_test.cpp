#include "kairo/ui/hit_test.h"

namespace kairo::ui {

static bool contains(const UiNode* n, int16_t x, int16_t y) {
    return x >= n->x && x < n->x + (int)n->w &&
           y >= n->y && y < n->y + (int)n->h;
}

static UiNode* hit(UiNode* n, int16_t x, int16_t y) {
    UiNode* found = nullptr;
    if (contains(n, x, y) && n->focusable && (n->onPress || n->onAdjust)) found = n;
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
