// Plan 60 Fase 3 — widgets rewrite using theme spacing.
// Node structure is unchanged (tier-0 preserved). Padding/gap now read from
// aether::theme() so compact/large theme tokens propagate to all components.
#include "nema/ui/widgets.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/text_style.h"
#include <vector>

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

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

// FocusStop — a non-interactive focus landmark (smart-scroll, Plan 79). It joins
// the flat focus traversal (Prev/Next can land on it and scroll it to the top) but
// renders no select highlight and has no Activate action. Wrap a trailing info
// block (header + read-only rows) in one so button nav can reach it instead of
// being stuck below the last selectable item.
UiNode* FocusStop(NodeArena& a, Style style, std::initializer_list<UiNode*> children) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type       = NodeType::View;
    n->style      = style;
    n->focusable  = true;
    n->focusInert = true;
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
    uint8_t pad = aether::theme().space.sm;  // themed padding (4px default, 2px compact)
    Style s; s.dir = FlexDir::Row; s.border = true; s.padding = pad;
    s.align = Align::Center; s.justify = Justify::Center;
    return Pressable(a, onPress, userdata, s, { Text(a, label, TextRole::Body) });
}

UiNode* Header(NodeArena& a, const char* title) {
    uint8_t gap = aether::theme().space.xs;
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
        n->style.padding    = aether::theme().space.sm;
    }
    return n;
}

UiNode* ListRow(NodeArena& a, const char* label, void (*onPress)(void*), void* userdata) {
    uint8_t pad = aether::theme().space.sm;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center;
    return Pressable(a, onPress, userdata, s, { Text(a, label, TextRole::Body) });
}

UiNode* ListItem(NodeArena& a, const char* label, const char* accessory,
                 void (*onPress)(void*), void* userdata) {
    uint8_t pad = aether::theme().space.sm;
    uint8_t gap = aether::theme().space.xs;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center; s.gap = gap;
    s.justify = Justify::SpaceBetween;
    UiNode* lbl = Text(a, label, TextRole::Body);
    if (lbl) lbl->style.flexGrow = 1;   // label grows → accessory pushed flush-right
    if (accessory && *accessory)
        return Pressable(a, onPress, userdata, s,
                         { lbl, Text(a, accessory, TextRole::Caption) });
    return Pressable(a, onPress, userdata, s, { lbl });
}

// ── Plan 79: Flipper-style list (Layer 3) ───────────────────────────────────

namespace {
// Fixed-size empty box used to inset row content horizontally/vertically without
// relying on uniform padding (which can't be left-only / top-only).
UiNode* hspace(NodeArena& a, uint16_t w) {
    Style s; s.width = w; s.height = 1;
    return View(a, s, {});
}
UiNode* vspace(NodeArena& a, uint16_t h) {
    Style s; s.height = h;
    return View(a, s, {});
}
// Row content height = body glyph cell + breathing room (Flipper rows ≈ 16px).
uint16_t listRowH() { return (uint16_t)(aether::ui::measureTextH(TextRole::Body) + 4); }
}  // namespace

UiNode* ListContainer(NodeArena& a, ScrollState& st,
                      std::initializer_list<UiNode*> rows) {
    Style sv; sv.dir = FlexDir::Col; sv.align = Align::Stretch;
    sv.flexGrow = 1; sv.padding = 2; sv.gap = 2;   // 2px inset = box edge offset
    return ScrollView(a, st, sv, rows);
}

UiNode* ListSection(NodeArena& a, const char* title) {
    // Bold section title (Subhead = Bold8), inset to align ~with row labels,
    // with a little space above it.
    Style col; col.dir = FlexDir::Col; col.align = Align::Start;
    Style row; row.dir = FlexDir::Row; row.align = Align::Center;
    return View(a, col, {
        vspace(a, 4),
        View(a, row, { hspace(a, 5), Text(a, title, TextRole::Subhead) }),
        vspace(a, 1),
    });
}

UiNode* ListItemRow(NodeArena& a, const ListEntry& e) {
    Style s; s.dir = FlexDir::Row; s.align = Align::Center;
    s.height = listRowH();
    s.selectBox = true;                 // rounded inverted box when focused
    s.gap = aether::theme().space.xs;     // 2px between label/value/chevron

    // label marquees on focus (TextRole::Smart) and grows to push the value right
    UiNode* label = SmartLabel(a, e.label ? e.label : "");
    if (label) label->style.flexGrow = 1;

    // Children: [5px inset] [icon?] label [value?] [chevron?] [right inset]
    UiNode* row = Pressable(a, e.onPress, e.user, s, {});
    UiNode* prev = nullptr;
    auto add = [&](UiNode* n) {
        if (!n) return;
        if (!prev) row->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };
    add(hspace(a, 5));                                  // label 5px inside the box
    if (e.leftIcon && e.iconW && e.iconH) {
        add(Icon(a, e.leftIcon, e.iconW, e.iconH, 0));
        add(hspace(a, 3));
    }
    add(label);
    if (e.value && *e.value) add(Text(a, e.value, TextRole::Body));
    if (e.chevron)           add(Text(a, ">", TextRole::Body));
    add(hspace(a, 4));                                  // clear the right rounding
    if (row) row->focusable = (e.onPress != nullptr);   // display-only rows aren't selectable
    return row;
}

UiNode* ListInputRow(NodeArena& a, const ListInput& e) {
    // FIXED split, content-independent: the label column and the value column are
    // flex-basis:0 grow children, so they divide the row by a constant ratio no
    // matter how long the label is — the split line, chevrons and value line up
    // on EVERY row. The label clips/marquees inside its column.
    constexpr uint16_t LEFT_W  = 11;   // label column ≈55% of the grow space
    constexpr uint16_t RIGHT_W = 9;    // value column ≈45%
    constexpr uint16_t CHEV_W  = 12;   // fixed chevron column (reserved either way)

    UiNode* label = SmartLabel(a, e.label ? e.label : "");
    if (label) { label->style.flexGrow = LEFT_W; label->style.flexZero = true; }

    // Fixed-width chevron columns — glyph centered; reserved even when not shown
    // so the value stays vertically aligned across rows.
    Style cv; cv.dir = FlexDir::Row; cv.align = Align::Center; cv.justify = Justify::Center;
    cv.width = CHEV_W;
    UiNode* lchev = View(a, cv, { e.canPrev ? Text(a, "<", TextRole::Body) : nullptr });
    UiNode* rchev = View(a, cv, { e.canNext ? Text(a, ">", TextRole::Body) : nullptr });

    // Value column — centered between the chevrons, also flex-basis:0.
    Style vc; vc.dir = FlexDir::Row; vc.align = Align::Center; vc.justify = Justify::Center;
    vc.flexGrow = RIGHT_W; vc.flexZero = true;
    UiNode* vbox = View(a, vc, { Text(a, e.value ? e.value : "", TextRole::Body) });

    // Flat row: [5px] label | < | value | > | [4px]
    Style s; s.dir = FlexDir::Row; s.align = Align::Center;
    s.height = listRowH(); s.selectBox = true;
    UiNode* n = View(a, s, { hspace(a, 5), label, lchev, vbox, rchev, hspace(a, 4) });
    if (n) { n->focusable = true; n->onAdjust = e.onAdjust; n->userdata = e.user; }
    return n;
}

// ── Native input controls ──────────────────────────────────────────────────

static UiNode* labelGrow(NodeArena& a, const char* label) {
    UiNode* t = Text(a, label, TextRole::Body);
    if (t) t->style.flexGrow = 1;
    return t;
}

UiNode* Toggle(NodeArena& a, const char* label, bool on,
               void (*onToggle)(void*), void* userdata) {
    uint8_t pad = aether::theme().space.sm;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center;
    s.justify = Justify::SpaceBetween;
    return Pressable(a, onToggle, userdata, s,
                     { Text(a, label, TextRole::Body),
                       Text(a, on ? "ON" : "OFF", TextRole::Caption) });
}

static UiNode* adjustRow(NodeArena& a, const char* label, const char* lo,
                         const char* value, const char* hi,
                         void (*onAdjust)(void*, int), void* userdata) {
    uint8_t pad = aether::theme().space.sm;
    uint8_t gap = aether::theme().space.xs;
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
    uint8_t pad = aether::theme().space.sm;
    uint8_t gap = aether::theme().space.xs;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center; s.gap = gap;
    return Pressable(a, onPress, userdata, s,
                     { labelGrow(a, label), Text(a, text, TextRole::Body) });
}

UiNode* Menu(NodeArena& a, const MenuItem* items, int count) {
    uint8_t gap = aether::theme().space.xs;
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
    uint8_t pad = aether::theme().space.md;
    uint8_t gap = aether::theme().space.sm;
    Style s; s.dir = FlexDir::Col; s.padding = pad; s.gap = gap;
    s.align = Align::Stretch; s.justify = Justify::Center;
    return View(a, s, children);
}

// ── Plan 70: Modal dialog widgets ─────────────────────────────────────────────

UiNode* Dialog(NodeArena& a, const char* title, const char* body,
               const uint8_t* icon, uint8_t iconW, uint8_t iconH,
               const DialogButton* buttons, uint8_t buttonCount) {
    uint8_t pad = aether::theme().space.md;
    uint8_t gap = aether::theme().space.sm;

    // Collect children: icon, title, body, button row
    std::vector<UiNode*> kids;
    if (icon && iconW > 0 && iconH > 0)
        kids.push_back(Icon(a, icon, iconW, iconH, 0));
    if (title && *title)
        kids.push_back(Text(a, title, TextRole::Title));
    if (body && *body)
        kids.push_back(Text(a, body, TextRole::Body));

    // Button row: Left | Center | Right
    if (buttons && buttonCount > 0 && buttonCount <= 3) {
        Style row; row.dir = FlexDir::Row; row.gap = gap;
        row.align = Align::Center; row.justify = Justify::Center;
        UiNode* btnRow = View(a, row, {});
        UiNode* prev = nullptr;
        for (uint8_t i = 0; i < buttonCount; i++) {
            UiNode* btn = Pressable(a, buttons[i].onClick, buttons[i].userdata,
                                    Style{}, {Text(a, buttons[i].label, TextRole::Body)});
            if (!btn) break;
            if (!prev) btnRow->firstChild = btn;
            else       prev->nextSibling = btn;
            prev = btn;
        }
        kids.push_back(btnRow);
    }

    // Wrap in a centered column container
    Style col; col.dir = FlexDir::Col; col.padding = pad; col.gap = gap;
    col.align = Align::Center;
    UiNode* root = View(a, col, {});
    UiNode* prev = nullptr;
    for (auto* k : kids) {
        if (!prev) root->firstChild = k;
        else       prev->nextSibling = k;
        prev = k;
    }
    return root;
}

UiNode* Popup(NodeArena& a, const char* text,
              const uint8_t* icon, uint8_t iconW, uint8_t iconH) {
    uint8_t pad = aether::theme().space.md;
    uint8_t gap = aether::theme().space.sm;

    Style col; col.dir = FlexDir::Col; col.padding = pad; col.gap = gap;
    col.align = Align::Center;
    UiNode* root = View(a, col, {});
    UiNode* prev = nullptr;

    if (icon && iconW > 0 && iconH > 0) {
        UiNode* ic = Icon(a, icon, iconW, iconH, 0);
        root->firstChild = ic;
        prev = ic;
    }
    if (text && *text) {
        UiNode* t = Text(a, text, TextRole::Body);
        if (!prev) root->firstChild = t;
        else       prev->nextSibling = t;
    }
    return root;
}

UiNode* Toast(NodeArena& a, const char* message) {
    uint8_t pad = aether::theme().space.sm;
    Style s; s.dir = FlexDir::Row; s.padding = pad; s.align = Align::Center;
    s.background = true;   // dark background, white text
    return View(a, s, {Text(a, message, TextRole::Caption)});
}

UiNode* AnimatedIcon(NodeArena& a, anim::AnimationPlayer& player) {
    UiNode* n = a.alloc();
    if (!n) return nullptr;
    n->type = NodeType::AnimatedIcon;
    n->animPlayer = &player;
    n->iconW = player.width();
    n->iconH = player.height();
    return n;
}

} // namespace aether::ui
