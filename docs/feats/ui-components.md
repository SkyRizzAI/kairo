# UI components — Switch, ProgressBar, SwitchRow, MenuBuilder — Feature Reference

Recent additions to the Aether widget catalog: a graphical on/off **Switch**, a read-only
**ProgressBar**, the **SwitchRow** list row, the fluent **MenuBuilder** for settings-style
screens, and a responsive **scrollbar reserve** so rows and right-aligned accessories stay
clear of the scrollbar.

For the rest of the toolkit (layout model, primitives, focus, scrolling) see
[`aether-ui.md`](aether-ui.md). Switch and ProgressBar are **native node types** shared by
all three runtimes (firmware / WASM simulator / JS `.papp` apps) — see
[ADR 0018](../decisions/0018-ui-component-runtime-parity.md). Focus behaviour (why
`MenuBuilder` takes a focus index) is covered by
[ADR 0017](../decisions/0017-ui-focus-model-react-rebuild-xor-highlight.md).

---

## Switch (native on/off)

A graphical on/off toggle accessory — a rounded track with a knob. It is a native node
(`NodeType::Switch`) drawn directly by the renderer, not a composition of primitives.

```cpp
UiNode* Switch(NodeArena& a, bool on);
```

Appearance (from `renderer.cpp`):

- **On** — **filled** track + a knob **hole** (unfilled rounded square) on the **right**.
- **Off** — **outline** track + a **filled** knob on the **left**.

The knob is drawn as a dark hole when on (rather than a filled pip) so it reads correctly
against the filled track. The knob diameter is `height − 4` (2px inset top/bottom); rounded
ends use r=3 for tracks ≥ 8px tall, r=2 otherwise. `Switch` itself is **not** focusable or
interactive — use it as a row accessory via `SwitchRow` (below) so the row owns the tap.

---

## ProgressBar (native read-only)

A read-only progress/usage indicator: a rounded outline track with a proportional fill.

```cpp
UiNode* ProgressBar(NodeArena& a, int pct);   // pct 0..100 (clamped)
```

Native node `NodeType::Progress`; the renderer draws a rounded outline and fills
`(width − 4) * pct / 100` px inside a 2px inset, clamping `pct` to 100. It is **not**
focusable or interactive — use it for capacity/usage/progress display, **not** a knob-less
`Slider` (a Slider is a control; a ProgressBar is an indicator). Inset like a list row so it
sits naturally under a "label  value" row.

---

## SwitchRow

A selectable list row with a `Switch` accessory on the right — the same height, insets and
rounded focus fill as every other `ListItemRow`.

```cpp
UiNode* SwitchRow(NodeArena& a, const char* label, bool on,
                  void (*onToggle)(void*), void* user);
```

Tap / `Activate` calls `onToggle`; the callback flips the caller's own bool, and the next
`build()` reflects it. Internally a `ListItemRow` whose `ListEntry::valueNode` is a `Switch`
(the `valueNode` accessory slot overrides plain `value` text on any list row).

---

## MenuBuilder

A fluent builder for settings-style lists. It wraps a `ListContainer` + the standard rows,
**auto-fills the `user` pointer once**, and lets a screen drop its per-screen
`static void xAdj(void*)` thunks — callbacks become inline lambdas at the call site.

```cpp
class MenuBuilder {
    MenuBuilder(NodeArena& a, ScrollState& scroll, void* user);

    MenuBuilder& section (const char* title);                    // non-focusable subheader
    MenuBuilder& info    (const char* label, const char* value);// non-focusable info row
    MenuBuilder& nav     (const char* label, void(*onPress)(void*));        // chevron nav row
    MenuBuilder& toggle  (const char* label, bool on, void(*onToggle)(void*)); // SwitchRow
    MenuBuilder& input   (const char* label, const char* value,
                          void(*onAdjust)(void*,int), bool canPrev=true, bool canNext=true);
    MenuBuilder& progress(int pct);                             // non-focusable ProgressBar
    MenuBuilder& add     (UiNode* node);                        // raw escape hatch
    UiNode*      build();
};
```

Usage:

```cpp
return MenuBuilder(a, scroll_, this)
    .section("Display")
    .input  ("Theme", themeName, [](void* u, int d){ S(u)->cycleTheme(d); })
    .toggle ("Dark",  darkOn_,   [](void* u){ S(u)->toggleDark(); })
    .nav    ("Desktop",          [](void* u){ S(u)->openDesktop(); })
    .progress(batteryPct_)
    .build();
```

- The `user` passed to the constructor is forwarded to every row's callback, so call sites
  never re-specify it.
- `section`, `info`, and `progress` add non-focusable rows; `nav`, `toggle`, and `input`
  add focusable rows. Use `add()` to drop in any pre-built node (its own `focusable`/`onPress`
  drives focus).
- **Focus look** is automatic — the renderer's XOR highlight marks the focused row, so the
  builder needs no per-row focus wiring. For the rare case of content that must *change* on
  focus (swap/animate an icon), do it with ordinary conditional rendering in `build()` — the
  screen rebuilds every frame and knows the focused index. See
  [ADR 0017](../decisions/0017-ui-focus-model-react-rebuild-xor-highlight.md).

---

## Responsive scrollbar reserve

When a scroll container shows its scrollbar, the layout engine **reserves
`SCROLLBAR_RESERVE` px** on the cross axis so rows never sit under the bar. Because the rows
are narrowed at layout time, the focus fill spans the (already-narrowed) full row width and
right-aligned accessories (value text, `Switch`, chevron) reposition to the left of the bar
automatically — no per-row scrollbar gutter is needed in the renderer. When the bar is
hidden (content fits), rows reclaim the full width.

The geometry constants live in `firmware/core/include/nema/ui/ui_constants.h` (one source of
truth shared by the layout engine that reserves the space and the renderer that draws the
bar):

```cpp
constexpr uint16_t SCROLLBAR_BAR_INSET = 3;   // bar drawn 3px in from the scroll node's right edge
constexpr uint16_t SCROLLBAR_RESERVE   = 5;   // cross-axis space reserved when the bar shows
```

---

## JS app usage (`.papp`)

`Switch` and `ProgressBar` are exposed to JS apps as typed intrinsics in the app SDK
(`@palanu/app-sdk`, `components.ts`) and mapped to their native nodes by the on-device JS
reconciler — same components the built-in screens use:

```tsx
import { Switch, ProgressBar } from "@palanu/app-sdk";

<Switch on={settings.dark} />
<ProgressBar pct={battery} />        // pct: 0..100 (clamped)
```

`Switch on` defaults to `false`; the reconciler gives it a default 18×9 box if no style size
is set. `ProgressBar pct` is required, clamped to 0..100, with a default height of 7px. Both
draw with the exact firmware/simulator pixels because they share the renderer
(see [ADR 0018](../decisions/0018-ui-component-runtime-parity.md)).
