// Plan 60 Fase 3 — widgets rewrite using theme spacing.
// Node structure is unchanged (tier-0 preserved). Padding/gap now read from
// nema::theme() so compact/large theme tokens propagate to all components.
#include "nema/ui/widgets.h"
#include "nema/ui/style_tokens.h"

namespace nema::ui {

NodeArena::NodeArena(size_t capacity) : cap_(capacity) {
    pool_ = new UiNode[capacity];
}

NodeArena::~NodeArena() { delete[] pool_; }

UiNode* NodeArena::alloc() {
    if (used_ >= cap_) return nullptr;
    UiNode* n = &pool_[used_++];
    *n = UiNode{};
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
    n->type     = NodeType::Pressable;
    n->style    = style;
    n->onPress  = onPress;
    n->userdata = userdata;
    n->focusable = true;
    setChildren(n, children);
    return n;
}

UiNode* ScrollView(NodeArena& a, ScrollState& st, Style style,
                   std::initializer_list<UiNode*> children) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type  = NodeType::Scroll;
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
    uint8_t pad = nema::theme().space.sm;  // themed padding (4px default, 2px compact)
    Style s; s.dir = FlexDir::Row; s.border = true; s.padding = pad;
    s.align = Align::Center; s.justify = Justify::Center;
    return Pressable(a, onPress, userdata, s, { Text(a, label, TextRole::Body) });
}

UiNode* Header(NodeArena& a, const char* title) {
    uint8_t gap = nema::theme().space.xs;
    Style col; col.dir = FlexDir::Col; col.align = Align::Stretch; col.gap = gap;
    Style line; line.height = 1; line.background = true;
    return View(a, col, { Text(a, title, TextRole::Title), View(a, line, {}) });
}

UiNode* Footer(NodeArena& a, const char* hint) {
    return Text(a, hint, TextRole::Caption);
}

UiNode* SmartLabel(NodeArena& a, const char* text) {
    return Text(a, text, TextRole::Smart);
}

UiNode* Icon(NodeArena& a, const uint8_t* bitmap, uint8_t w_px, uint8_t h_px,
             uint8_t padding) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type        = NodeType::Icon;
    n->iconBitmap  = bitmap;
    n->iconW       = w_px;
    n->iconH       = h_px;
    n->style.padding = padding;
    return n;
}

UiNode* TitleBar(NodeArena& a, const char* title) {
    // A Title-role Text node with a filled background; the renderer fills the
    // box and draws the glyphs inverted (white). Themed padding; stretches full
    // width when placed in a Stretch Col.
    UiNode* n = Text(a, title, TextRole::Title);
    if (n) {
        n->style.background = true;
        n->style.padding    = nema::theme().space.sm;
    }
    return n;
}

UiNode* ListRow(NodeArena& a, const char* label, void (*onPress)(void*), void* userdata) {
    uint8_t pad = nema::theme().space.sm;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center;
    return Pressable(a, onPress, userdata, s, { Text(a, label, TextRole::Body) });
}

UiNode* ListItem(NodeArena& a, const char* label, const char* accessory,
                 void (*onPress)(void*), void* userdata) {
    uint8_t pad = nema::theme().space.sm;
    uint8_t gap = nema::theme().space.xs;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center; s.gap = gap;
    s.justify = Justify::SpaceBetween;
    UiNode* lbl = Text(a, label, TextRole::Body);
    if (lbl) lbl->style.flexGrow = 1;   // label grows → accessory pushed flush-right
    if (accessory && *accessory)
        return Pressable(a, onPress, userdata, s,
                         { lbl, Text(a, accessory, TextRole::Caption) });
    return Pressable(a, onPress, userdata, s, { lbl });
}

// ── Native input controls ──────────────────────────────────────────────────

static UiNode* labelGrow(NodeArena& a, const char* label) {
    UiNode* t = Text(a, label, TextRole::Body);
    if (t) t->style.flexGrow = 1;
    return t;
}

UiNode* Toggle(NodeArena& a, const char* label, bool on,
               void (*onToggle)(void*), void* userdata) {
    uint8_t pad = nema::theme().space.sm;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center;
    s.justify = Justify::SpaceBetween;
    return Pressable(a, onToggle, userdata, s,
                     { Text(a, label, TextRole::Body),
                       Text(a, on ? "ON" : "OFF", TextRole::Caption) });
}

static UiNode* adjustRow(NodeArena& a, const char* label, const char* lo,
                         const char* value, const char* hi,
                         void (*onAdjust)(void*, int), void* userdata) {
    uint8_t pad = nema::theme().space.sm;
    uint8_t gap = nema::theme().space.xs;
    Style row; row.dir = FlexDir::Row; row.padding = pad; row.align = Align::Center;
    row.gap = gap;
    UiNode* n = View(a, row, {
        labelGrow(a, label),
        Text(a, lo,    TextRole::Caption),
        Text(a, value, TextRole::Body),
        Text(a, hi,    TextRole::Caption),
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
    return n;
}

UiNode* TextField(NodeArena& a, const char* label, const char* text,
                  void (*onPress)(void*), void* userdata) {
    uint8_t pad = nema::theme().space.sm;
    uint8_t gap = nema::theme().space.xs;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center; s.gap = gap;
    return Pressable(a, onPress, userdata, s,
                     { labelGrow(a, label), Text(a, text, TextRole::Body) });
}

UiNode* Menu(NodeArena& a, const MenuItem* items, int count) {
    uint8_t gap = nema::theme().space.xs;
    Style col; col.dir = FlexDir::Col; col.align = Align::Stretch; col.gap = gap;
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
    uint8_t pad = nema::theme().space.md;
    uint8_t gap = nema::theme().space.sm;
    Style s; s.dir = FlexDir::Col; s.padding = pad; s.gap = gap;
    s.align = Align::Stretch; s.justify = Justify::Center;
    return View(a, s, children);
}

} // namespace nema::ui
