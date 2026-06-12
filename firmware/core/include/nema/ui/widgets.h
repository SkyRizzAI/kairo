#pragma once
#include "nema/ui/node.h"
#include <cstddef>
#include <initializer_list>

namespace nema::ui {

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

// ScrollView: a scroll container. Children stack along style.dir (default Col =
// vertical scroll). The viewport is BOUNDED by the parent flex layout (the node
// claims leftover space via flexGrow, default 1); content longer than the
// viewport overflows and scrolls. `st` is caller-owned and persists scroll
// position across frames. Renderer clips to the viewport + draws a scrollbar.
UiNode* ScrollView(NodeArena& a, ScrollState& st, Style style,
                   std::initializer_list<UiNode*> children = {});

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

// ListRow: a full-width focusable row with a left-aligned label and no border —
// selection is shown by the focus ring (buttons) or touch. Put rows in a Col or
// ScrollView with align=Stretch so they fill the width. The list/menu idiom.
UiNode* ListRow(NodeArena& a, const char* label, void (*onPress)(void*), void* userdata);

// ── Native input controls (Plan 30/31) ────────────────────────────────────
// All are composed from primitives (except Slider, a native node) so the same
// shapes map cleanly to a future TSX/JS reconciler.

// Toggle / Switch: a focusable row "label      [ON]/[OFF]". Activate/tap flips
// (caller's onPress reads/writes its own bool, then the next build() reflects it).
UiNode* Toggle(NodeArena& a, const char* label, bool on,
               void (*onToggle)(void*), void* userdata);

// Stepper: "label   - value +". A SINGLE focusable control: Left/Right (or tap
// the −/+ half) calls onAdjust(dir); Activate advances (+1) as the fallback on
// boards without horizontal keys. `value` is a caller-formatted string.
UiNode* Stepper(NodeArena& a, const char* label, const char* value,
                void (*onAdjust)(void* u, int dir), void* userdata);

// Select / cycle: "label   < value >". Same interaction model as Stepper:
// Left/Right is the primary adjust; Up/Down only moves focus between rows; tap a
// half or Activate works when the board has no Left/Right.
UiNode* Select(NodeArena& a, const char* label, const char* value,
               void (*onAdjust)(void* u, int dir), void* userdata);

// Slider: a native track/fill/knob bound to a caller-owned int in [min,max].
// Touch: drag to set. Buttons: focus + Left/Right adjust by `step`. onChange
// fires on every change. Wrap with a label in a Row/Col as needed.
UiNode* Slider(NodeArena& a, int* value, int min, int max, int step,
               void (*onChange)(void*, int), void* userdata);

// TextField: a focusable row "label: <text>" that fires onPress (the caller
// opens an editor, e.g. the VirtualKeyboard, at app level). Inline display only.
UiNode* TextField(NodeArena& a, const char* label, const char* text,
                  void (*onPress)(void*), void* userdata);

// Menu: a Col of Buttons, one per item. Each item carries its own callback +
// userdata (the C-idiomatic way — encode an index in userdata if you like:
// userdata = (void*)(intptr_t)i with a shared onPress that casts it back).
struct MenuItem {
    const char* label;
    void      (*onPress)(void* userdata);
    void*       userdata;
};
UiNode* Menu(NodeArena& a, const MenuItem* items, int count);

// Modal: the content box for a centered overlay dialog. Return this from
// ComponentApp::buildModal() — the runtime centers it, draws a white backdrop +
// border behind it, and routes focus/input to it (the base freezes). Lay out a
// prompt + a Row of Buttons inside. Size defaults to ~¾×½ of the screen; set
// style.width/height on the result to override. Padding/gap/Stretch preset.
UiNode* Modal(NodeArena& a, std::initializer_list<UiNode*> children = {});

} // namespace nema::ui
