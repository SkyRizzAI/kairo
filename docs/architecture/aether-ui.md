# Aether UI — Architecture

Aether is the default display server (`AetherServer`) and widget toolkit for Palanu.
It operates entirely on a 1-bit `Canvas` — no second framebuffer, no compositing
layer. This document describes the internal rendering pipeline, memory model, and
how the major subsystems interact.

---

## Component tree pipeline

Every frame follows a fixed three-phase sequence:

```
build() → layout() → render()
```

### 1. build()

The screen or app subclass allocates `UiNode` objects from its `NodeArena` and
returns the root node:

```cpp
UiNode* build(NodeArena& a, Runtime& rt);   // ComponentScreen
UiNode* build(NodeArena& a, AppContext& ctx); // ComponentApp
```

`build()` must be **pure read** with respect to layout results — it only reads
application state and writes the tree. It must not cache returned node pointers for
use across frame boundaries.

The base calls `arena_.reset()` before invoking `build()`, which is an O(1) pointer
reset with no per-node destructor overhead.

### 2. layout()

`renderComponentFrame()` (in `component_runtime.h`) calls the layout engine:

- Recursively measures each node bottom-up (content-size from children or intrinsic
  text/icon metrics).
- Distributes main-axis space top-down, respecting `flexGrow`, `flexZero`, and
  `min/max` constraints.
- Applies margins per side (`mt/mr/mb/ml`) after flex distribution.
- Writes absolute `x, y, w, h` in logical pixels onto each `UiNode` in the tree.
- For absolute-positioned nodes (`Position::Absolute`), the node is removed from the
  flex flow and placed at `(parent.x + absX, parent.y + absY)`.

After layout, `ScrollState::contentMain` and `viewportMain` are filled; `scrollMain`
is clamped to `[0, maxScroll()]`.

### 3. render()

The renderer walks the tree depth-first. For each node:

- Clips to the node's bounding box (enforced for `ScrollView` content).
- Draws background/border if `style.background` or `style.border`.
- Draws text with the correct font and role, applying marquee scroll for `TextRole::Smart`
  when the node is focused.
- For `Icon`: blits the XBM bitmap.
- For `AnimatedIcon`: calls `player->currentFrameData()` and blits the frame.
- For `Slider`: draws track, fill, and knob at the position mapped from `[min, max]`.
- For focused `Pressable` with `style.selectBox = true`: paints a rounded (r=1)
  inverted inset box before the children — the Flipper-style row highlight.
- For `VirtualList` items with `selfHighlight = true`: applies the same XOR inversion
  without being in the focus tree.

`renderComponentFrame()` is called from `ComponentScreen::draw()` and from
`ComponentApp::run()`'s inner loop.

---

## NodeArena memory model

`NodeArena` is a fixed-capacity stack allocator:

```
pool_[ 0 .. used_-1 ]  — live nodes for this frame
pool_[ used_ .. cap_-1 ] — free
sentinel_              — returned on overflow (never null)
```

`reset()` sets `used_ = 0` and clears the overflow flag. There is no per-node
deallocation. The pool is a flat array allocated once at construction.

On overflow, `alloc()` returns `&sentinel_` (a zero-initialised `UiNode`) and logs
once per frame. Widgets that receive the sentinel silently produce empty/invisible
content, so the UI degrades gracefully rather than crashing.

Default capacities: `ComponentScreen` = 256, `ComponentApp` = 256 (overridable via
`arenaCapacity()`). `AppListScreen` uses 512 to accommodate the VirtualList spacers.

---

## VirtualList rendering

`VirtualList` creates a `ScrollView` whose content is a window of `renderItem` results
plus two spacer nodes:

```
[top spacer]         ← height = focusedIndex * itemH (gives scrollbar correct thumb)
[visible items ± 1]  ← only these are built each frame
[bottom spacer]      ← height = remaining items * itemH
```

Only items within `[scrollMain / itemH, (scrollMain + viewportH) / itemH + overscan]`
are passed to `renderItem`. Items outside that window consume no arena space.

The VirtualList forces `focusable = false` on every internal node so
`ComponentRuntime`'s focus tree and `VirtualListState`'s index-based focus never
interfere. The screen drives focus entirely through `vst.moveFocus(±1)` and signals
the highlight via `node->selfHighlight = focused` in `renderItem`.

`VirtualListState::scrollToFocused()` adjusts `scrollMain` so the focused item
remains within the viewport — called automatically by `moveFocus()`.

---

## Canvas clip-based transition rendering

Transitions use two draw passes against the same `Canvas` with different clip regions.
There is no offscreen buffer.

```
tick 1..8:
  splitX = (tick / kTransitionTicks) * canvasWidth
  SlideLeft:  to-screen draws [0, splitX), from-screen draws [splitX, W)
  SlideRight: to-screen draws [splitX, W), from-screen draws [0, splitX)
```

`AetherServer` tracks `transitionTick_` (0 = idle; 1–8 = animating). Each call to
`renderFrame()` advances the tick. `Canvas::clear()` respects the active clip, so
each screen's draw clears and redraws only its half of the display.

The "from" screen is the one that was active before navigate/goBack; it is stored by
`ViewDispatcher` as `transitionFrom_` and consumed by `AetherServer` each frame.

No transition state leaks to the next frame: after tick 8, `clearTransition()` resets
`transitionType_` to `None` and nulls `transitionFrom_`.

---

## AetherServer

`AetherServer` is the `IDisplayServer` implementation that owns the overall render
loop for a frame:

1. **Theme install**: calls `setTheme()` with the active `StyleTokens` before any
   draw. Screens and widgets read font metrics and token values through the theme.
2. **Status bar**: draws the top status strip (time, battery, icons) from `StatusBarData`.
3. **Screen render**: calls the active `IScreen::draw(canvas)` (which invokes
   `renderComponentFrame` for component screens).
4. **Modal backdrop**: when `ViewDispatcher::previous()` is not null and the active
   screen is `ScreenMode::Modal`, draws a dither pattern over the background before
   rendering the modal content.
5. **Transition**: if `pendingTransition() != None`, runs the two-pass clip render
   described above.
6. **FPS overlay**: if `showFps_` is true, overlays the measured frame rate in the
   corner (calculated over a rolling 1-second window of completed flushes).

---

## ComponentScreen base

`ComponentScreen` wraps a `NodeArena` and a `ComponentState` and implements `IScreen`:

- `draw(Canvas&)`: resets the arena, calls `build()`, then `renderComponentFrame()`.
  Skips rebuild when `dirty_ = false` (model unchanged).
- `onAction(Action)`: feeds navigation through `dispatchNav()` / `dispatchAdjust()`.
  Back action calls `onBack()` (deprecated) or triggers `rt_.view().goBack()`.
- `onPointer(PointerEvent&)`: feeds touch through `dispatchPointer()`.
- `tick(nowMs)`: calls `tickMomentum(state_)` for flick momentum and rate-limits
  marquee redraws via `lastMarqueeMs_`.
- `onResume()`: called by `ViewDispatcher` when the screen becomes active; subclasses
  typically reset scroll and focus here.

`ComponentState` persists across frames and holds:
- `FocusState focus` — index of the currently focused node in document order.
- `InputModality modality` — Button or Touch (determines focus ring visibility).
- Tap and drag tracking for touch hit-testing.
- Active slider drag geometry (captured on Down so the drag is stable after a rebuild).

---

## Logging requirement

All logging inside `AetherServer`, `ComponentScreen`, `ComponentApp`, and all widget
implementations MUST go through `rt.log()`. Raw `printf`, `Serial.println`, and
`ESP_LOGx` bypass the ConsoleSink/MemorySink fan-out and must not be used. See
`CLAUDE.md` for the two sanctioned exceptions (pre-runtime boot and test harnesses).
