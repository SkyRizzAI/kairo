#pragma once
// Plan 50 — Toolkit nema::ui (node-tree + flexbox + widget builders).
// This is an OPTIONAL library, NOT a universal UI contract. Individual display
// servers MAY use it (Aether does), but are NOT required to (LVGL would use its
// own lv_obj tree). It is NOT part of the shared System API (Plan 48).
// Status: library reuse, not interface standard.
#include "nema/ui/node.h"
#include "nema/ui/animation_player.h"
#include <cstddef>
#include <initializer_list>

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

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

// TitleBar (Plan 60): a full-width FILLED title bar (banner) with the title in
// inverted (white) Title-role text. Use as the first child of a Stretch Col so
// it spans the width. The renderer fills the box (rounded) and draws white text.
UiNode* TitleBar(NodeArena& a, const char* title);

// Footer: a Caption-role hint line (use at the bottom of a Col).
UiNode* Footer(NodeArena& a, const char* hint);

// SmartLabel (Plan 52): a Text node with TextRole::Smart. Ellipsis when its
// parent Pressable is not focused; marquee-scrolls when focused. Use inside a
// ListItem or ListRow to handle long strings gracefully.
UiNode* SmartLabel(NodeArena& a, const char* text);

// Icon (Plan 53): a 1-bit XBM bitmap leaf node. Pass the bitmap pointer and
// its pixel dimensions; the node sizes itself to w_px×h_px plus padding.
// Use findIcon() from icon_pack.h to look up built-in bitmaps by handle.
UiNode* Icon(NodeArena& a, const uint8_t* bitmap, uint8_t w_px, uint8_t h_px,
             uint8_t padding = 0);

// ListRow: a full-width focusable row with a left-aligned label and no border —
// selection is shown by the focus ring (buttons) or touch. Put rows in a Col or
// ScrollView with align=Stretch so they fill the width. The list/menu idiom.
UiNode* ListRow(NodeArena& a, const char* label, void (*onPress)(void*), void* userdata);

// ListItem (Plan 60): a focusable row "label                accessory". The
// label grows to fill, pushing the accessory (e.g. ">", a value, "ON"/"OFF")
// flush-right. accessory may be nullptr for none. Themed padding. Use inside a
// ScrollView (align=Stretch) — the renderer draws a dashed scrollbar for free.
UiNode* ListItem(NodeArena& a, const char* label, const char* accessory,
                 void (*onPress)(void*), void* userdata);

// ── Plan 79: Flipper-style list (Layer 3 component) ─────────────────────────
// Built FROM Layer-2 primitives (Pressable/Text/Icon/Spacer). Put rows inside a
// ListContainer (a ScrollView preset: align=Stretch, 2px inset, gap). The focused
// row paints a rounded inset selection box (Style::selectBox) and its label
// marquees when too long to fit. Tokens match the reference: box inset 2px from
// the screen edge, label 5px inside the box, subheaders in bold.

// ListContainer: the scroll viewport for list rows — preset padding/inset/gap so
// rows + selection box sit at the right offsets. `st` is caller-owned (scroll pos).
UiNode* ListContainer(NodeArena& a, ScrollState& st,
                      std::initializer_list<UiNode*> rows = {});

// ListSection: a bold section subheader (not selectable). Place between groups
// of rows; may appear at the top or anywhere mid-list.
UiNode* ListSection(NodeArena& a, const char* title);

// One selectable list row. Fields beyond `label` are optional:
//   value    — right-aligned value text (e.g. "15s", "<default>")
//   leftIcon — XBM drawn before the label (file/app lists); set iconW/iconH
//   chevron  — append a ">" affordance at the far right
struct ListEntry {
    const char*    label    = nullptr;
    const char*    value    = nullptr;
    const uint8_t* leftIcon = nullptr;
    uint8_t        iconW    = 0;
    uint8_t        iconH    = 0;
    bool           chevron  = false;
    void         (*onPress)(void*) = nullptr;
    void*          user     = nullptr;
};
UiNode* ListItemRow(NodeArena& a, const ListEntry& e);

// ListInputRow: the split "label  < value >" row (Flipper variable-item style).
// The row is divided at a center point — label fills the left half, value the
// right half — each CLIPPED to its half and marquee-scrolling when the row is
// focused and the text overflows the half. Chevrons appear per canPrev/canNext.
// Focusable: Left/Right calls onAdjust(dir) to change the value.
struct ListInput {
    const char* label    = nullptr;
    const char* value    = nullptr;
    bool        canPrev  = true;    // show left chevron "<"
    bool        canNext  = true;    // show right chevron ">"
    void      (*onAdjust)(void* u, int dir) = nullptr;
    void*       user     = nullptr;
};
UiNode* ListInputRow(NodeArena& a, const ListInput& e);

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

// ── Plan 70: Modal dialog widgets ─────────────────────────────────────────

// DialogButton: a single button in a Dialog.
struct DialogButton {
    const char* label;
    void      (*onClick)(void*);
    void*       userdata;
};

// Dialog: a full modal dialog with title, body, optional icon, and up to 3
// buttons (Left/Center/Right). The dialog is vertically centered and surrounded
// by a rounded box. Use inside a screen with ScreenMode::Modal.
//   - title: centered, Title-role (large bold)
//   - body:  centered, Body-role, supports \n for multiline
//   - icon:  can be nullptr for no icon
//   - buttons: nullptr or array of DialogButton, buttonCount 1-3
UiNode* Dialog(NodeArena& a, const char* title, const char* body,
               const uint8_t* icon = nullptr, uint8_t iconW = 0, uint8_t iconH = 0,
               const DialogButton* buttons = nullptr, uint8_t buttonCount = 0);

// Popup: a lightweight notification popup with icon + text + optional auto-
// dismiss timeout. Simpler than Dialog — single action, no button array.
UiNode* Popup(NodeArena& a, const char* text,
              const uint8_t* icon = nullptr, uint8_t iconW = 0, uint8_t iconH = 0);

// Toast: a non-blocking bottom-of-screen notification bar. Passes input through
// to the screen behind it. Designed for system notifications.
UiNode* Toast(NodeArena& a, const char* message);

// ── Plan 70: Animated Icon ────────────────────────────────────────────────

// AnimatedIcon: a leaf node that draws the current frame of an AnimationPlayer.
// The player is caller-owned (lives as a screen/app member). The renderer
// calls animPlayer->currentFrameData() each frame.
UiNode* AnimatedIcon(NodeArena& a, anim::AnimationPlayer& player);

} // namespace aether::ui
