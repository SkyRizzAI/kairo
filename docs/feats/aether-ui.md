# Aether UI — Feature Reference

Aether is Palanu's built-in 1-bit UI system: a retained-mode widget tree with a
flexbox layout engine, rendered directly onto the Canvas without a second framebuffer.
It is the default display server (`AetherServer`) and the backing system for all
system screens and apps that extend `ComponentScreen` / `ComponentApp`.

---

## NodeArena

Every widget tree is built from a `NodeArena` — a fixed-capacity stack allocator
that lives as a member of the screen or app.

```cpp
NodeArena arena_(256);   // capacity: 256 UiNode objects
```

Rules:
- Call `arena_.reset()` at the start of each frame (the base classes do this automatically).
- `alloc()` returns a zero-reset `UiNode*`. On overflow it returns a non-null
  **sentinel node** (renders nothing), logs once, and sets `overflowed()`.
- Do **not** `new UiNode` per frame and do not cache returned pointers across frames —
  the arena is wiped on each `reset()`.
- Override `arenaCapacity()` in a `ComponentApp` subclass, or pass the capacity
  to the `ComponentScreen` constructor, when a large tree is needed.

---

## Layout model

The layout engine is a single-axis flexbox. Every `UiNode` carries a `Style`:

| Field | Type | Meaning |
|---|---|---|
| `dir` | `FlexDir::Col / Row` | Stack children vertically or horizontally |
| `flexGrow` | `uint16_t` | Weight for distributing leftover main-axis space (0 = no grow) |
| `flexZero` | `bool` | Ignore the node's content size when distributing grow space (fixed-ratio splits) |
| `width / height` | `uint16_t` | Fixed size; `SIZE_AUTO` (0xFFFF) = measure from content |
| `minW/maxW/minH/maxH` | `uint16_t` | Clamp after flex distribution |
| `padding` | `uint8_t` | Uniform 4-side inner padding |
| `gap` | `uint8_t` | Spacing between children along the main axis |
| `align` | `Align` | Cross-axis alignment: `Start / Center / End / Stretch` |
| `justify` | `Justify` | Main-axis distribution: `Start / Center / End / SpaceBetween` |
| `mt/mr/mb/ml` | `int8_t` | Per-side margins (applied after flex) |
| `border` | `bool` | Draw a 1px outline around the node's bounding box |
| `background` | `bool` | Fill the node's bounding box |
| `selectBox` | `bool` | Rounded inset highlight box when this focusable node is focused |
| `position` | `Position::Relative / Absolute` | Absolute removes node from flex flow |
| `absX / absY` | `int16_t` | Absolute position relative to parent top-left |

A typical full-screen root:

```cpp
Style root;
root.dir      = FlexDir::Col;
root.flexGrow = 1;
root.align    = Align::Stretch;
return View(a, root, { TitleBar(a, "TITLE"), ListContainer(a, scroll_, { ... }) });
```

---

## Widget catalog

### Primitive builders

| Widget | Description |
|---|---|
| `View(a, style, children)` | Generic flex container — the building block for all layouts |
| `Row(a, style, children)` | View with `dir = FlexDir::Row` preset |
| `Col(a, style, children)` | View with `dir = FlexDir::Col` preset |
| `Text(a, str, role)` | Text leaf. Roles: `Body`, `Title`, `Caption`, `Mono`, `Smart`, `Subhead` |
| `Pressable(a, onPress, ud, style, children)` | Focusable container; fires `onPress` on Activate/tap |
| `ScrollView(a, st, style, children)` | Scrollable viewport; content clips and scrolls; draws a scrollbar |
| `Container(a, padding, children)` | Col that fills its parent (`flexGrow=1`) with uniform padding |
| `Button(a, label, onPress, ud)` | Focusable pressable with a border and centered label |
| `Header(a, title)` | Title text followed by a full-width separator |
| `TitleBar(a, title)` | Full-width filled banner with inverted (white) title text |
| `Footer(a, hint)` | Caption-role hint line for the bottom of a screen |
| `SmartLabel(a, text)` | Ellipsis when the parent Pressable is not focused; marquees when focused |
| `Icon(a, bitmap, w, h, padding)` | 1-bit XBM bitmap leaf |
| `AnimatedIcon(a, player)` | Draws the current frame of a caller-owned `AnimationPlayer` |

### List layer (Flipper-style)

| Widget | Description |
|---|---|
| `ListContainer(a, scroll, rows)` | ScrollView preset for list rows — correct padding, inset, and gap |
| `ListSection(a, title)` | Bold non-selectable section header; place between row groups |
| `ListItemRow(a, entry)` | Selectable row with label, optional right-aligned value, left icon, chevron |
| `ListInputRow(a, input)` | Split label/value row with `<`/`>` chevrons; Left/Right calls `onAdjust` |

`ListEntry` fields:

```cpp
struct ListEntry {
    const char*    label    = nullptr;  // required
    const char*    value    = nullptr;  // right-aligned value text
    const uint8_t* leftIcon = nullptr;  // XBM bitmap
    uint8_t        iconW    = 0;
    uint8_t        iconH    = 0;
    bool           chevron  = false;    // show ">" affordance
    void         (*onPress)(void*) = nullptr;
    void*          user     = nullptr;
};
```

`ListInput` fields:

```cpp
struct ListInput {
    const char* label   = nullptr;
    const char* value   = nullptr;
    bool        canPrev = true;   // show "<"
    bool        canNext = true;   // show ">"
    void      (*onAdjust)(void* u, int dir) = nullptr;
    void*       user    = nullptr;
};
```

### Input controls

| Widget | Description |
|---|---|
| `Toggle(a, label, on, onToggle, ud)` | Focusable on/off row; Activate flips, callback reads/writes caller's bool |
| `Stepper(a, label, value, onAdjust, ud)` | `label  - value +`; Left/Right or Activate calls `onAdjust(dir)` |
| `Select(a, label, value, onAdjust, ud)` | `label  < value >`; same interaction as Stepper |
| `Slider(a, value, min, max, step, onChange, ud)` | Track/fill/knob bound to a caller-owned `int`; drag or Left/Right to adjust |
| `TextField(a, label, text, onPress, ud)` | Focusable `label: text` row; Activate fires `onPress` (caller opens editor) |
| `Menu(a, items, count)` | Column of Buttons, one per `MenuItem` |

### Modal / dialog

| Widget | Description |
|---|---|
| `Modal(a, children)` | Content box for a centered overlay; used inside `ComponentApp::buildModal()` |
| `Dialog(a, title, body, icon, iconW, iconH, buttons, n)` | Full modal: title, body, optional icon, up to 3 buttons |
| `Popup(a, text, icon, iconW, iconH)` | Lightweight single-action notification |
| `Toast(a, message)` | Non-blocking bottom notification bar; passes input through to the screen behind it |

`DialogButton`:

```cpp
struct DialogButton {
    const char* label;
    void      (*onClick)(void*);
    void*       userdata;
    bool        danger = false;   // filled/inverted button for destructive actions
};
```

### Skeleton placeholders

| Widget | Description |
|---|---|
| `SkeletonRow(a, phase, width)` | Animated dashed-rect placeholder while list data loads |
| `SkeletonBlock(a, w, h, phase)` | Fixed-size skeleton for image/card placeholders |

---

## Focus and navigation

`ComponentRuntime` manages a focus ring across all focusable nodes in the tree.

- **Button modality**: `Prev` / `Next` move focus through the tree in document order.
  `Activate` fires the focused node's `onPress`. The focused node auto-scrolls into
  view inside any `ScrollView` ancestor.
- **Touch modality**: tapping a `Pressable` fires `onPress` directly; dragging scrolls
  the nearest `ScrollView`.
- The focus ring (selection highlight) is only visible in Button modality.
- `Style::selectBox = true` draws a Flipper-style rounded inset box instead of a
  full-bleed XOR invert — used automatically by `ListItemRow`.

### VirtualList focus (app-managed)

When using `VirtualList`, focus is managed entirely by the app via `VirtualListState`,
not by `ComponentRuntime`. All internal nodes have `focusable = false`. The focused
item signals its highlight via `node->selfHighlight = focused` inside `renderItem`.

---

## ScrollState and ListContainer

`ScrollState` is a persistent, caller-owned struct that stores scroll position across
frames:

```cpp
struct ScrollState {
    int16_t  scrollMain   = 0;   // current offset (px)
    uint16_t contentMain  = 0;   // total content length (filled by layout)
    uint16_t viewportMain = 0;   // visible length (filled by layout)
    float    velocity     = 0;   // momentum (set on flick release)
    int16_t  maxScroll() const;
};
```

`ListContainer` is a `ScrollView` preset wired to a `ScrollState`. Declare one per
scrollable section as a member of your screen or app:

```cpp
aether::ui::ScrollState scroll_;   // persists across frames
```

---

## VirtualList

`VirtualList` renders only the visible window of items — suitable when the list is
large (> ~20 items) or when arena overflow is a concern.

```cpp
UiNode* VirtualList(NodeArena& a, VirtualListState& vst,
                    int totalCount, uint16_t itemHeight,
                    UiNode* (*renderItem)(NodeArena&, int, bool, void*),
                    void* userdata, Style style = {});
```

`VirtualListState` extends `ScrollState` with a `focusedIndex`. Navigation is
entirely app-driven:

```cpp
case input::Action::Prev:
    if (vlist_.moveFocus(-1)) requestRedraw();
    break;
case input::Action::Next:
    if (vlist_.moveFocus(+1)) requestRedraw();
    break;
case input::Action::Activate:
    handleSelect(vlist_.focusedIndex);
    break;
```

Inside `renderItem`, apply highlight via `selfHighlight`:

```cpp
static UiNode* renderItem(NodeArena& a, int i, bool focused, void* ud) {
    ListEntry e; e.label = ...; e.chevron = true;
    UiNode* row = ListItemRow(a, e);
    if (row) row->selfHighlight = focused;
    return row;
}
```

---

## Dialog / Modal pattern

For system-level confirmation dialogs, use `Dialog` directly as the return value of
`ComponentScreen::build()`:

```cpp
UiNode* MyModal::build(NodeArena& a, Runtime&) {
    DialogButton btns[2] = {
        {"Confirm", onConfirm, this},
        {"Cancel",  onCancel,  this},
    };
    return Dialog(a, "Are you sure?", "This cannot be undone.",
                  nullptr, 0, 0, btns, 2);
}
```

For in-app modals (overlaid on the running app), override `buildModal()` in a
`ComponentApp` subclass and return a `Modal(a, { ... })` node. The runtime centers it,
draws a dithered backdrop, and routes all input to it while the base is frozen.

---

## NodeRef — inspecting layout results

`NodeRef` gives access to a node's computed geometry after layout:

```cpp
NodeRef ref;
auto* btn = withRef(Button(a, "OK", cb, ud), ref);
// After renderComponentFrame: ref.x(), ref.y(), ref.w(), ref.h() are valid.
scrollIntoView(ref, scroll_);   // adjust scroll so btn is visible
```

`NodeRef` is invalidated on the next `arena_.reset()`.

---

## Transitions

Screen navigation can carry an animation:

```cpp
rt_.view().navigate(nextScreen_, Transition::SlideLeft);
rt_.view().goBack(Transition::SlideRight);
```

| Value | Effect |
|---|---|
| `Transition::None` | Instant cut (default) |
| `Transition::SlideLeft` | New screen slides in from the right (forward navigation) |
| `Transition::SlideRight` | New screen slides in from the left (back gesture) |

Transitions run for 8 ticks using a two-pass clip-based renderer — no second
framebuffer is required.

---

## AnimatedValue

Spring-physics scalar for smooth value animation (scroll positions, progress bars,
slide offsets):

```cpp
AnimatedValue val;
val.animateTo(targetY);            // set target
if (val.tick(dtMs)) requestRedraw(); // advance; returns true while still moving
float y = val.value;               // read current position
val.snapTo(v);                     // instant, no spring
```

Default spring: stiffness 200, damping 20. Pass custom values to `animateTo(t, k, b)`.

`AnimatedValue` is not suitable for 1-bit opacity blending — use `Transition` for
screen-level fades/slides.

---

## Logging

All logging inside screens and apps MUST use `rt.log()` / `rt_.log()`. Raw
`printf`, `Serial`, or `ESP_LOGx` are not permitted outside the sanctioned boot and
test exceptions defined in `CLAUDE.md`.

```cpp
rt_.log().info("MyScreen", "resumed");
rt_.log().error("MyScreen", "arena overflow detected");
```
