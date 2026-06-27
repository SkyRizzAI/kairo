#pragma once
// Plan 50 — Toolkit aether::ui (node-tree + flexbox + widget builders).
// This is an OPTIONAL library, NOT a universal UI contract. Individual display
// servers MAY use it (Aether does), but are NOT required to (LVGL would use its
// own lv_obj tree). It is NOT part of the shared System API (Plan 48).
// Status: library reuse, not interface standard.
#include "nema/ui/node.h"
#include "nema/ui/animation_player.h"
#include "nema/ui/animated_value.h"
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

    UiNode* alloc();    // returns a zero-reset node; returns &sentinel_ on overflow (log once per frame, never null)
    void    reset();    // call at the start of each render() — O(1); also resets the overflow flag

    size_t  used()     const { return used_; }
    size_t  capacity() const { return cap_; }

    bool   overflowed()    const { return overflowLogged_; }
    size_t overflowCount() const { return overflowCount_; }

private:
    UiNode* pool_ = nullptr;
    size_t  cap_  = 0;
    size_t  used_ = 0;

    bool   overflowLogged_ = false;
    size_t overflowCount_  = 0;
    UiNode sentinel_{};  // returned instead of nullptr on overflow; renders nothing
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

// ── Footer legends (Plan 92) ────────────────────────────────────────────────
// A bottom soft-key / hint bar: a row of filled pill buttons, each a small icon
// + short caption (Flipper-style "Mission" / "Launcher" legends). Layout follows
// the item count:
//   • 1 item  → hugs the left edge (Justify::Start)
//   • 2+ items → spread edge-to-edge (Justify::SpaceBetween)
// Place the result inside a Stretch parent (or set its style.width to the canvas
// width) so the row has full-width slack for space-between to distribute.
//
// Each pill is a filled rounded capsule whose icon + label render in paper colour
// (inverted), so they read as white-on-dark on mono / orange-on-dark on Flipper.
// Items are tappable when onPress is set, but are NOT focus stops (a hint bar,
// like a soft-key strip — the physical key it labels does the real activation).
struct LegendItem {
    const uint8_t* icon  = nullptr;            // 1-bit XBM, optional (see findIcon())
    uint8_t        iconW = 0, iconH = 0;
    const char*    label = nullptr;            // short caption, optional
    void         (*onPress)(void*) = nullptr;  // tap handler (touch boards), optional
    void*          user  = nullptr;
};
UiNode* FooterLegends(NodeArena& a, const LegendItem* items, int count);

// ── Footer-legend collapse animation (Plan 92, Phase 2) ──────────────────────
// Caller-owned state (lives OUTSIDE the arena, like ScrollState) that drives the
// "icon + label → icon only" collapse. On show, every pill displays its full
// label; after `collapseDelayMs` the labels tween their width to 0 so each pill
// shrinks to just its icon — while space-between keeps the row's alignment.
//
// Lifecycle:
//   FooterLegendsState st;
//   void onResume() { st.reset(count); }          // re-arm: labels show again
//   void tick(dtMs) { if (st.tick(dtMs)) redraw; } // advance springs
//   void draw()     { FooterLegends(a, items, n, &st); }  // build with reveal
// Time-driven (NOT spring) so the collapse is wall-clock identical on every
// platform regardless of frame rate. A spring integrates per-frame, so on a
// low-FPS device it settles far slower (and can go unstable) than at 60 FPS —
// this uses elapsed milliseconds and a fixed eased duration instead.
struct FooterLegendsState {
    static constexpr int kMax = 6;
    float elapsedMs         = 0.f;       // wall-clock ms since reset()
    float collapseDelayMs   = 2000.f;    // labels stay full for this long
    float collapseDurationMs = 320.f;    // then collapse over this long
    int   count             = 0;

    // Re-arm: show all labels in full, restart the timer.
    void reset(int n) { count = n < kMax ? n : kMax; elapsedMs = 0.f; }

    bool settled() const { return elapsedMs >= collapseDelayMs + collapseDurationMs; }

    // Per-pill label reveal in [0,1] (1=full label, 0=icon only). Currently
    // uniform across pills; the index is kept for a future per-pill stagger.
    float reveal(int /*i*/) const {
        if (elapsedMs <= collapseDelayMs) return 1.f;
        float t = (elapsedMs - collapseDelayMs) / collapseDurationMs;
        if (t >= 1.f) return 0.f;
        float e = t * t * (3.f - 2.f * t);   // smoothstep ease-in-out
        return 1.f - e;
    }

    // Advance the wall-clock timer by dtMs. Returns true while a redraw is still
    // needed (during the collapse window); false once settled (idle).
    bool tick(float dtMs) {
        if (settled()) return false;
        elapsedMs += dtMs;
        return elapsedMs > collapseDelayMs;   // only the collapse window animates
    }
};

// Animated overload: same layout as FooterLegends() but each pill's label width
// is scaled by st.reveal[i] (and dropped once it collapses to ~0px), producing
// the Phase-2 collapse. Pass the same `count` used for reset().
UiNode* FooterLegends(NodeArena& a, const LegendItem* items, int count,
                      FooterLegendsState& st);

// Paragraph — multi-line wrapped text block (like HTML <p>). Fills its container's width
// and auto-sizes height by word-wrapping (the layout recomputes height from the stretched
// width). Put it in a stretch container (ListContainer / a Col with align=Stretch). Use for
// long text: addresses, private keys (role=Mono), descriptions.
UiNode* Paragraph(NodeArena& a, const char* text, TextRole role = TextRole::Body);

// SmartLabel (Plan 52): a Text node with TextRole::Smart. Ellipsis when its
// parent Pressable is not focused; marquee-scrolls when focused. Use inside a
// ListItemRow to handle long strings gracefully.
UiNode* SmartLabel(NodeArena& a, const char* text);

// Icon (Plan 53): a 1-bit XBM bitmap leaf node. Pass the bitmap pointer and
// its pixel dimensions; the node sizes itself to w_px×h_px plus padding.
// Use findIcon() from icon_pack.h to look up built-in bitmaps by handle.
UiNode* Icon(NodeArena& a, const uint8_t* bitmap, uint8_t w_px, uint8_t h_px,
             uint8_t padding = 0);

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
    const char*    label     = nullptr;
    const char*    value     = nullptr;
    UiNode*        valueNode = nullptr;  // custom right-side accessory (e.g. a Switch); overrides value
    const uint8_t* leftIcon  = nullptr;
    uint8_t        iconW     = 0;
    uint8_t        iconH     = 0;
    bool           chevron   = false;
    void         (*onPress)(void*) = nullptr;
    void*          user      = nullptr;
};
UiNode* ListItemRow(NodeArena& a, const ListEntry& e);

// Spinner — a native animated busy indicator (3-dot comet orbiting). Square `size` px.
// Animates from the global render tick, so keep the screen redrawing while it's shown.
UiNode* Spinner(NodeArena& a, uint16_t size = 13);

// Switch — a graphical on/off toggle accessory (rounded track + knob: knob left = off,
// right = on). Use as a row's right-side accessory via SwitchRow (consistent list look).
UiNode* Switch(NodeArena& a, bool on);

// SwitchRow — a list row with a Switch accessory on the right. Same rounded focus fill,
// height and insets as every other ListItemRow. Tap (Activate) calls onToggle.
UiNode* SwitchRow(NodeArena& a, const char* label, bool on,
                  void (*onToggle)(void*), void* user);

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

// ── MenuBuilder ────────────────────────────────────────────────────────────
// Ergonomic fluent builder for settings-style lists. Wraps ListContainer + the standard
// rows and auto-fills the `user` pointer once, so a screen stops re-declaring per-screen
// input/nav/sw lambda helpers and stops writing named `static void xAdj(void*)` thunks
// (callbacks are inline lambdas at the call site). The focused row's look is handled
// automatically by the renderer's XOR highlight — no per-row focus wiring needed.
//
//   #define S(u) static_cast<MyScreen*>(u)
//   return MenuBuilder(a, scroll_, this)
//       .section("Display")
//       .input ("Theme",  themeName, [](void* u, int d){ S(u)->cycleTheme(d); })
//       .toggle("Dark",   darkOn_,   [](void* u){ S(u)->toggleDark(); })
//       .nav   ("Desktop",           [](void* u){ S(u)->openDesktop(); })
//       .build();
class MenuBuilder {
public:
    MenuBuilder(NodeArena& a, ScrollState& scroll, void* user);

    MenuBuilder& section(const char* title);                                  // non-focusable subheader
    MenuBuilder& info   (const char* label, const char* value);              // non-focusable info row
    MenuBuilder& nav    (const char* label, void (*onPress)(void*));         // chevron nav row
    MenuBuilder& toggle (const char* label, bool on, void (*onToggle)(void*));
    MenuBuilder& input  (const char* label, const char* value,
                         void (*onAdjust)(void*, int), bool canPrev = true, bool canNext = true);
    MenuBuilder& progress(int pct);                                          // non-focusable ProgressBar
    MenuBuilder& add    (UiNode* node);                                      // raw escape hatch

    UiNode* build();   // returns the root View wrapping the list

private:
    void appendRow(UiNode* n);
    NodeArena&   a_;
    void*        user_;
    UiNode*      list_;          // the ListContainer (Scroll) node
    UiNode*      tail_ = nullptr;
};

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

// ProgressBar (read-only): a rounded outline track with a proportional fill (0..100%).
// NOT focusable or interactive — for capacity/usage/progress display. Use this instead of
// a knob-less Slider (a Slider is a control; a progress bar is an indicator). Inset like a
// list row so it sits under a "label  value" row.
UiNode* ProgressBar(NodeArena& a, int pct);

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
    bool        danger = false;   // true → inverted (filled) button for destructive actions
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

// ── Plan 90: Skeleton placeholders ────────────────────────────────────────────

// SkeletonRow: an animated placeholder row used while list data is loading
// (e.g. LazyDirLoader is still fetching). Renders as a dashed rect that
// toggles solid/outline every `phase` ticks. Pass a counter incremented each
// tick (e.g. a uint32_t member of the screen) for the animation phase.
// When `phase` is always 0 the row renders as a static dashed outline.
UiNode* SkeletonRow(NodeArena& a, uint32_t phase = 0, uint16_t width = SIZE_AUTO);

// SkeletonBlock: a fixed-size skeleton placeholder (image, card, etc.).
UiNode* SkeletonBlock(NodeArena& a, uint16_t w, uint16_t h, uint32_t phase = 0);

// ── Plan 90 F5.2: NodeRef — safe handle to a laid-out UiNode ─────────────────
//
// A NodeRef is filled by the layout engine after build(), pointing to the
// specific node in the arena. Valid only until the next build() (arena reset).
// Use withRef() to attach a ref to any node. After renderComponentFrame(),
// the ref is filled with the node's computed x/y/w/h.
//
// Use cases:
//   - scrollIntoView(ref, scroll_) — adjust scroll so ref is visible
//   - Inspect computed geometry for overlays / absolute positioning
struct NodeRef {
    UiNode* node = nullptr;
    bool valid() const { return node != nullptr; }
    int16_t  x() const { return node ? node->x : 0; }
    int16_t  y() const { return node ? node->y : 0; }
    uint16_t w() const { return node ? node->w : 0; }
    uint16_t h() const { return node ? node->h : 0; }
};

// Attach a NodeRef to a node. The ref is filled after layout().
// Usage: auto* btn = withRef(Button(a, "OK", cb, ud), myRef);
UiNode* withRef(UiNode* node, NodeRef& ref);

// Scroll a ScrollState so that ref's node is fully visible in the viewport.
// Call after layout (e.g. inside renderComponentFrame or after build).
// Returns true if scroll position changed.
bool scrollIntoView(const NodeRef& ref, ScrollState& st);

} // namespace aether::ui
