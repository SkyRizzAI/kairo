#include "kairo/ui/widgets.h"

namespace kairo::ui {

NodeArena::NodeArena(size_t capacity) : cap_(capacity) {
    pool_ = new UiNode[capacity];
}

NodeArena::~NodeArena() { delete[] pool_; }

UiNode* NodeArena::alloc() {
    if (used_ >= cap_) return nullptr;
    UiNode* n = &pool_[used_++];
    *n = UiNode{};   // reset to defaults
    return n;
}

void NodeArena::reset() { used_ = 0; }

void setChildren(UiNode* parent, std::initializer_list<UiNode*> children) {
    UiNode* prev = nullptr;
    for (UiNode* child : children) {
        if (!child) continue;
        if (!prev) parent->firstChild = child;
        else       prev->nextSibling  = child;
        prev = child;
    }
}

UiNode* View(NodeArena& a, Style style, std::initializer_list<UiNode*> children) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type  = NodeType::View;
    n->style = style;
    setChildren(n, children);
    return n;
}

UiNode* Text(NodeArena& a, const char* str, TextRole role) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type = NodeType::Text;
    n->text = str;
    n->role = role;
    return n;
}

UiNode* Pressable(NodeArena& a, void (*onPress)(void*), void* userdata,
                  Style style, std::initializer_list<UiNode*> children) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type      = NodeType::Pressable;
    n->style     = style;
    n->onPress   = onPress;
    n->userdata  = userdata;
    n->focusable = true;
    setChildren(n, children);
    return n;
}

// ── Mid-level ──────────────────────────────────────────────────────────────

UiNode* Row(NodeArena& a, Style style, std::initializer_list<UiNode*> children) {
    style.dir = FlexDir::Row;
    return View(a, style, children);
}

UiNode* Col(NodeArena& a, Style style, std::initializer_list<UiNode*> children) {
    style.dir = FlexDir::Col;
    return View(a, style, children);
}

UiNode* Container(NodeArena& a, uint8_t padding, std::initializer_list<UiNode*> children) {
    Style s; s.dir = FlexDir::Col; s.flexGrow = 1; s.padding = padding;
    return View(a, s, children);
}

UiNode* Button(NodeArena& a, const char* label, void (*onPress)(void*), void* userdata) {
    Style s; s.dir = FlexDir::Row; s.border = true; s.padding = 2;
    s.align = Align::Center; s.justify = Justify::Center;
    return Pressable(a, onPress, userdata, s, { Text(a, label, TextRole::Body) });
}

UiNode* Header(NodeArena& a, const char* title) {
    Style col; col.dir = FlexDir::Col; col.align = Align::Stretch; col.gap = 2;
    Style line; line.height = 1; line.background = true;   // full-width separator
    return View(a, col, { Text(a, title, TextRole::Title), View(a, line, {}) });
}

UiNode* Footer(NodeArena& a, const char* hint) {
    return Text(a, hint, TextRole::Caption);
}

UiNode* Menu(NodeArena& a, const MenuItem* items, int count) {
    Style col; col.dir = FlexDir::Col; col.align = Align::Stretch; col.gap = 2;
    UiNode* menu = View(a, col, {});
    UiNode* prev = nullptr;
    for (int i = 0; i < count; i++) {
        UiNode* btn = Button(a, items[i].label, items[i].onPress, items[i].userdata);
        if (!btn) break;
        if (!prev) menu->firstChild = btn;
        else       prev->nextSibling = btn;
        prev = btn;
    }
    return menu;
}

} // namespace kairo::ui
