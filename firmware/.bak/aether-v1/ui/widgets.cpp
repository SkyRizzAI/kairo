#include "nema/ui/widgets.h"

namespace nema::ui {

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

UiNode* ScrollView(NodeArena& a, ScrollState& st, Style style,
                   std::initializer_list<UiNode*> children) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type  = NodeType::Scroll;
    // Default to filling the available space so the parent flex bounds the
    // viewport (content overflows & scrolls). Callers can override flexGrow/size.
    if (style.flexGrow == 0 && style.width == SIZE_AUTO && style.height == SIZE_AUTO)
        style.flexGrow = 1;
    n->style  = style;
    n->scroll = &st;
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

UiNode* ListRow(NodeArena& a, const char* label, void (*onPress)(void*), void* userdata) {
    Style s; s.dir = FlexDir::Row; s.padding = 2; s.align = Align::Center;
    return Pressable(a, onPress, userdata, s, { Text(a, label, TextRole::Body) });
}

// ── Native input controls ──────────────────────────────────────────────────

// A label Text that grows to push trailing controls to the right edge.
static UiNode* labelGrow(NodeArena& a, const char* label) {
    UiNode* t = Text(a, label, TextRole::Body);
    if (t) t->style.flexGrow = 1;
    return t;
}

UiNode* Toggle(NodeArena& a, const char* label, bool on,
               void (*onToggle)(void*), void* userdata) {
    Style s; s.dir = FlexDir::Row; s.padding = 2; s.align = Align::Center;
    s.justify = Justify::SpaceBetween;
    return Pressable(a, onToggle, userdata, s,
                     { Text(a, label, TextRole::Body), Text(a, on ? "[ON]" : "[OFF]", TextRole::Body) });
}

// A single focusable row whose value is tuned with Left/Right (onAdjust). The
// glyphs (`lo`/`hi`, e.g. "-"/"+" or "<"/">") are visual affordances; the whole
// row is one focus/tap target (location-aware tap: left half = −1, right = +1).
static UiNode* adjustRow(NodeArena& a, const char* label, const char* lo,
                         const char* value, const char* hi,
                         void (*onAdjust)(void*, int), void* userdata) {
    Style row; row.dir = FlexDir::Row; row.padding = 2; row.align = Align::Center; row.gap = 4;
    UiNode* n = View(a, row, {
        labelGrow(a, label),
        Text(a, lo, TextRole::Body),
        Text(a, value, TextRole::Body),
        Text(a, hi, TextRole::Body),
    });
    if (n) { n->focusable = true; n->onAdjust = onAdjust; n->userdata = userdata; }
    return n;
}

UiNode* Stepper(NodeArena& a, const char* label, const char* value,
                void (*onAdjust)(void* u, int dir), void* userdata) {
    return adjustRow(a, label, "-", value, "+", onAdjust, userdata);
}

UiNode* Select(NodeArena& a, const char* label, const char* value,
               void (*onAdjust)(void* u, int dir), void* userdata) {
    return adjustRow(a, label, "<", value, ">", onAdjust, userdata);
}

UiNode* Slider(NodeArena& a, int* value, int min, int max, int step,
               void (*onChange)(void*, int), void* userdata) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type        = NodeType::Slider;
    n->focusable   = true;
    n->sliderValue = value;
    n->sliderMin   = (int16_t)min;
    n->sliderMax   = (int16_t)max;
    n->sliderStep  = (int16_t)step;
    n->onChange    = onChange;
    n->userdata    = userdata;
    // Width comes from a Stretch parent (Col) or an explicit flexGrow in a Row.
    return n;
}

UiNode* TextField(NodeArena& a, const char* label, const char* text,
                  void (*onPress)(void*), void* userdata) {
    Style s; s.dir = FlexDir::Row; s.padding = 2; s.align = Align::Center; s.gap = 4;
    return Pressable(a, onPress, userdata, s,
                     { labelGrow(a, label), Text(a, text, TextRole::Body) });
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

UiNode* Modal(NodeArena& a, std::initializer_list<UiNode*> children) {
    // No background/border here — ComponentApp paints the white backdrop + border
    // and centers this box. Just lay the content out inside with breathing room.
    Style s; s.dir = FlexDir::Col; s.padding = 6; s.gap = 6;
    s.align = Align::Stretch; s.justify = Justify::Center;
    return View(a, s, children);
}

} // namespace nema::ui
