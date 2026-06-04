#include "kairo/ui/renderer.h"
#include "kairo/ui/text_style.h"
#include "kairo/ui/canvas.h"

namespace kairo::ui {

static void paint(const UiNode* n, Canvas& c, const UiNode* focused) {
    const Style& s = n->style;

    if (s.background) c.fillRect(n->x, n->y, n->w, n->h, true);
    if (s.border)     c.drawRect(n->x, n->y, n->w, n->h, true);

    if (n->type == NodeType::Text && n->text) {
        FontSpec fs = fontForRole(n->role);
        c.setFont(*fs.font);
        uint16_t tx = (uint16_t)(n->x + s.padding);
        uint16_t ty = (uint16_t)(n->y + s.padding);
        if (fs.scale <= 1) c.drawText(tx, ty, n->text);
        else               c.drawTextScaled(tx, ty, n->text, fs.scale);
    }

    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        paint(k, c, focused);

    // Focus highlight drawn last (XOR over the node's box).
    if (focused && n == focused)
        c.invertRect(n->x, n->y, n->w, n->h);
}

void render(const UiNode& root, Canvas& c, const UiNode* focused) {
    paint(&root, c, focused);
}

} // namespace kairo::ui
