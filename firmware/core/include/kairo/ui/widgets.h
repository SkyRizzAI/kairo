#pragma once
#include "kairo/ui/node.h"
#include <cstddef>
#include <initializer_list>

namespace kairo::ui {

// NodeArena — one fixed pool of UiNodes per app/screen. Build the whole tree
// from it each render, then reset() at the start of the next render. O(1) reset,
// zero per-node heap churn. NEVER `new UiNode` per frame.
class NodeArena {
public:
    explicit NodeArena(size_t capacity);
    ~NodeArena();
    NodeArena(const NodeArena&) = delete;
    NodeArena& operator=(const NodeArena&) = delete;

    UiNode* alloc();    // returns a zero-reset node, or nullptr if pool exhausted
    void    reset();    // call at the start of each render() — O(1)

    size_t  used()     const { return used_; }
    size_t  capacity() const { return cap_; }

private:
    UiNode* pool_ = nullptr;
    size_t  cap_  = 0;
    size_t  used_ = 0;
};

// ── Primitive builders (low-level) ─────────────────────────────────────────
// Children are linked via firstChild/nextSibling in list order.

UiNode* View(NodeArena& a, Style style, std::initializer_list<UiNode*> children = {});

UiNode* Text(NodeArena& a, const char* str, TextRole role = TextRole::Body);

UiNode* Pressable(NodeArena& a, void (*onPress)(void*), void* userdata,
                  Style style, std::initializer_list<UiNode*> children = {});

// Internal helper: link a list of children onto a parent node.
void setChildren(UiNode* parent, std::initializer_list<UiNode*> children);

// ── Mid-level components (built FROM primitives) ───────────────────────────

// Row / Col: like View but force the flex direction (style.dir overridden).
UiNode* Row(NodeArena& a, Style style, std::initializer_list<UiNode*> children = {});
UiNode* Col(NodeArena& a, Style style, std::initializer_list<UiNode*> children = {});

// Container: a Col that fills its parent (flexGrow=1) with uniform padding.
UiNode* Container(NodeArena& a, uint8_t padding, std::initializer_list<UiNode*> children = {});

// Button: a focusable Pressable with a border and a centered label.
UiNode* Button(NodeArena& a, const char* label, void (*onPress)(void*), void* userdata);

// Header: a Title line followed by a full-width separator. align=Stretch parent.
UiNode* Header(NodeArena& a, const char* title);

// Footer: a Caption-role hint line (use at the bottom of a Col).
UiNode* Footer(NodeArena& a, const char* hint);

// Menu: a Col of Buttons, one per item. Each item carries its own callback +
// userdata (the C-idiomatic way — encode an index in userdata if you like:
// userdata = (void*)(intptr_t)i with a shared onPress that casts it back).
struct MenuItem {
    const char* label;
    void      (*onPress)(void* userdata);
    void*       userdata;
};
UiNode* Menu(NodeArena& a, const MenuItem* items, int count);

} // namespace kairo::ui
