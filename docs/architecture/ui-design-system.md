# Palanu — UI Design System Reference

> **Purpose of this document.** A single, design-oriented consolidation of how the
> Palanu/Kairo UI actually works **today** — physical constraints, design tokens,
> typography, layout rules, the visual language, the component vocabulary, the screen
> map, and the rules a designer must obey. It is meant to be a self-contained briefing
> for a **final production UI concept** discussion (including with another AI). It does
> not invent a future look; it documents the present one and names the open design
> questions.
>
> **Source of truth is the code.** Every fact below cites the file it came from. When
> code and this doc disagree, the code wins — fix the doc.
>
> **Deep-dive companions** (do not duplicate their detail here):
> [`aether-ui.md`](aether-ui.md) (render pipeline internals),
> [`font-system.md`](font-system.md) (font format & loading),
> [`shell-desktop-launcher.md`](shell-desktop-launcher.md) (shell flow),
> [`how-to-write-a-screen.md`](how-to-write-a-screen.md) (authoring pattern),
> [`virtual-keyboard.md`](virtual-keyboard.md), [`assets.md`](assets.md).

---

## 1. The physical canvas — constraints that govern every design decision

These are non-negotiable and shape everything. A designer must internalise them first.

| Aspect | Reality in firmware | Design implication |
|---|---|---|
| **Colour depth** | **1-bit monochrome** — every pixel is on/off. The display server (`AetherServer`) renders directly to a 1-bit `Canvas`; there is no second framebuffer, no compositor, no greyscale. | No colour, no gradients, no shadows, no alpha. Hierarchy comes only from **inversion (XOR), 1-px borders, whitespace/spacing, and bold/large type.** |
| **Resolution** | SkyRizz-E32 is **240×320 logical px, portrait**. But code never assumes this — it draws from `canvas.width()/height()`. | Mock-ups must be **resolution-independent**. Design on a *logical* grid, not a fixed 240×320 bitmap. The same layout must survive other board sizes/scales. |
| **Scaling** | `Canvas` works in **logical pixels**; an integer-ish `serverScale()` maps logical→physical per board (`canvas.h`, `display_server.h`). | 1 logical unit ≠ 1 physical pixel necessarily. Never hand-place anything at a physical coordinate. |
| **Text grid** | Base metrics `CHAR_W = 6px` (5px glyph + 1px), `CHAR_H = 9px` (8px glyph + 1px) — `ui_constants.h`. | Vertical rhythm is effectively locked to ~9px rows. A 320px-tall screen fits ≈ 26 text rows minus chrome. |
| **Corners** | `fillRoundRect`/`drawRoundRect` only clip a **1-px corner** (Flipper style) — `canvas.h`. | "Rounded" is a subtle 1px chamfer, not a large radius. `RadiusTokens` are essentially decorative on 1-bit (see §4). |
| **Rendering model** | Immediate redraw of a retained node tree each frame; dirty-region + partial redraw for efficiency (`view_dispatcher.h`). | Animations are cheap only if small/region-bound. Full-screen motion is two-pass clip rendering (see §10), not free. |

**Source files:** `firmware/core/include/nema/ui/canvas.h`, `ui_constants.h`,
`display_server.h`, `view_dispatcher.h`.

---

## 2. Rendering architecture (what a design must map onto)

Palanu UI is **retained-mode / declarative**. Screens and apps describe a tree of
nodes every frame; the engine lays it out and paints it. Designers should think in
**flex containers + leaf widgets**, not in absolute pixel pushing.

```
build()  →  layout()  →  render()        (per frame, see aether-ui.md)
```

- **build()** — the screen/app allocates `UiNode`s from a `NodeArena` (a per-frame pool,
  O(1) reset, no heap churn) and returns the root. Pure: reads app state, writes tree.
- **layout()** — a two-pass flexbox subset writes absolute `x,y,w,h` (logical px) onto
  every node (`layout.h`).
- **render()** — depth-first paint to the 1-bit `Canvas` (`renderer.h`).

**AetherServer frame composition order** (`aether-ui.md` §AetherServer):
1. Install active **theme** (`StyleTokens`).
2. Draw **status bar** (top strip) from `StatusBarData`.
3. Draw the active **screen** (`IScreen::draw`).
4. If active screen is **Modal** and there's a screen beneath → **dither backdrop** then modal.
5. If a **transition** is pending → two-pass clip slide.
6. Optional **FPS overlay**.

> **Design takeaway:** the status bar, modal backdrop dither, and transitions are
> *system-owned chrome* — a design concept defines their look but the composition order
> is fixed.

**Source files:** `aether-ui.md`, `component_screen.h`, `component_runtime.h`,
`node.h`, `layout.h`, `renderer.h`, `widgets.h`.

---

## 3. The visual language of 1-bit (how hierarchy is expressed)

Because there is no colour, the system has a small, consistent set of "ink" devices.
A final design should standardise *when* each is used.

| Device | Mechanism | Current usage |
|---|---|---|
| **Inversion / selection** | XOR a region (`invertRect`); focused `Pressable` with `style.selectBox` paints a rounded (r=1) inverted box (`node.h`, `renderer.h`). | The primary "this is selected/focused" affordance for list rows and buttons. |
| **Self-highlight** | `selfHighlight=true` applies the same XOR without being in the focus tree (`node.h` F6.04). | VirtualList rows (app list) that manage focus by index. |
| **Filled banner** | `TitleBar` / `banner()` draws a filled bar with inverted (white-on-black) centred title (`draw.h`, `widgets.h`). | Page titles. `invertedStatusBar` token toggles filled vs outlined status bar. |
| **1-px frame / separator** | `frame()`, `box()`, `separator()` (`draw.h`). | Card borders, header underline, content zone dividers. |
| **Dither / backdrop** | `AetherServer` draws a dither pattern behind modals. | Signals "background is inactive" under a modal. |
| **Dashed track** | Scrollbar track is dashed, thumb is solid (`draw.h scrollbar()`). | Flipper-style scroll indicator. |
| **Ellipsis vs marquee** | `TextRole::Smart`: ellipsis when unfocused+overflowing, horizontal marquee when focused (`text_style.h`, `draw.h marquee()`). | Long labels in lists. |
| **Position dots** | `posbar()` 2×2 dots, current filled (`draw.h`). | Carousel/page position. |

**Source files:** `firmware/core/include/nema/ui/draw.h`, `renderer.h`, `node.h`.

---

## 4. Design tokens (`StyleTokens`) — the themeable backbone

This is the most important section for a "final design system": Palanu **already has a
design-token layer** (Plan 53). A theme is set once before the GUI starts and read-only
for the session; a swap restyles the whole UI without touching screen code. A production
design language maps cleanly onto these tokens.

**File:** `firmware/core/include/nema/ui/style_tokens.h`

### Spacing scale (`SpaceTokens`, logical px)
| Token | Value | Meaning |
|---|---|---|
| `xs` | **2** | icon inner pad, tiny inline gap |
| `sm` | **4** | row padding, small gap between items |
| `md` | **8** | section gap, card inner padding |
| `lg` | **12** | screen-edge margin |
| `xl` | **16** | large section gap, hero spacing |

### Radius scale (`RadiusTokens`, px)
`none=0`, `sm=2`, `md=4`, `full=255` (pill). **Always effectively 0 on 1-bit** —
present for a future colour display; do not rely on it visually now.

### Icon slot sizes (`IconTokens`, px)
| Token | Value | Use |
|---|---|---|
| `xs` | **8** | status-bar icons |
| `sm` | **12** | small inline icons |
| `md` | **16** | list-row icons |
| `lg` | **24** | launcher grid icons |
| `xl` | **32** | hero icon |

### Font render scale (`FontTokens`, multiplier on base bitmap)
`caption=1`, `body=1`, `title=2`, `subhead=1`. (Title is pixel-doubled until multi-size
fonts land — see §5.)

### Status bar style
`invertedStatusBar` (bool) — filled dark band vs outlined bar.

### Built-in themes
`defaultTheme()` (standard), `compactTheme()` (tighter spacing, small displays),
`largeTheme()` (bigger targets, accessibility). A production concept would either tune
`defaultTheme()` or add a new named `StyleTokens` instance.

> **Open design question:** the token set today covers **spacing, radius, icon size,
> font scale, status-bar fill**. It does **not** yet encode things like row height,
> selection style, border weight, or motion timing as tokens. A "final" system may want
> to extend `StyleTokens` so those are themeable too.

---

## 5. Typography

Two complementary systems: **roles** (semantic) resolve to **font handles** (concrete
fonts) at a **scale**.

### TextRole (semantic intent) — `node.h`
| Role | Intent |
|---|---|
| `Body` | normal text, list rows |
| `Title` | large heading / page title bar |
| `Caption` | small hint/label, footers |
| `Mono` | monospace — logs, hex, terminal |
| `Smart` | ellipsis when unfocused, marquee when focused (long labels) |
| `Subhead` | bold section header, smaller than Title (ListView sections) |

`fontForRole(role)` → `{handle, scale}`; both layout (measure) and renderer (paint) go
through it so sizes always match (`text_style.h`). A global `setTextSize(Normal|Large)`
shifts the mapping for accessibility.

### Font registry (concrete fonts) — `font_registry.h`
Role handles: `Primary` (bold, subhead/title), `Secondary` (regular body/list),
`Mono`, `Tiny`, `BigNum` (clocks). Plus explicit metrics: `Reg8/Bold8`, `Reg10/Bold10`,
`Reg12/Bold12`. Custom/loadable fonts start at handle `CUSTOM_BASE=16` (max 20).

### Font families shipped
- **Helvetica** proportional — `Reg/Bold` at 8/10/12px (`FONT_REG8`…`FONT_BOLD12`).
- **IoskeleyMono** — `Reg/Bold` at 8/10/12px (`FONT_IOSK_*`).
- Legacy monospace `FONT_5X8`, `FONT_6X8` (the base grid font).

Fonts are column-major bitmap fonts, proportional (per-glyph width) since Plan 79.
Generated from BDF/TTF via `tools/fonts/ttf_encode.py`. Font **packs** can be loaded at
runtime from the filesystem (`applyFontPack`) — i.e. the type can be re-skinned per
theme/asset pack. See [`font-system.md`](font-system.md) for the format.

> **Open design question:** larger type currently relies on **pixel-doubling** the base
> font (blocky) rather than true larger glyphs at every role. A production concept that
> wants crisp large headings should plan native larger font assets (Plan 25 Phase 3).

---

## 6. Layout system

A **subset of flexbox**, single main-axis per container. This is the only layout model;
a design must be expressible as nested rows/columns.

### `Style` properties available per node (`node.h`)
- **Direction:** `dir` = `Row` | `Col`.
- **Sizing:** `width`/`height` (`SIZE_AUTO` or fixed logical px); `flexGrow` (weight for
  leftover main-axis space); `flexZero` (flex-basis:0 — split by pure ratio ignoring
  content width, used for fixed label/value splits); `minW/maxW/minH/maxH` constraints.
- **Spacing:** `padding` (uniform 4-side); `gap` (between children); per-side margins
  `mt/mr/mb/ml`.
- **Alignment:** `align` (cross-axis: Start/Center/End/Stretch); `justify` (main-axis:
  Start/Center/End/SpaceBetween).
- **Decoration:** `border` (1-px outline), `background` (fill), `selectBox` (focus
  highlight style), `clip` (overflow-hidden — clip a node's drawing to its bbox).
- **Animatable text width:** `widthScale` (0..1) scales an auto-width `Text` node's
  measured width in layout, so a label can shrink without the caller measuring text
  (paired with `clip` for the footer-legend collapse).
- **Positioning:** `position` = Relative | Absolute (`absX/absY` pin to parent origin).
- **Text wrap:** `wrap` + `lineGap` + `maxLines` for multiline.

**Not supported:** flex-wrap (multi-line flow of children), grid, percentage sizing.
Design within nested single-axis flex only.

### Screen zones & resolution-independent constants (`ui_constants.h`)
```
CHAR_W = 6   CHAR_H = 9                 (font-relative, fixed)
STATUS_Y = 3   STATUS_H = 9            (status bar, top-anchored)
SEP1_Y   = 12  CONTENT_Y = 14          (content starts here when status bar ON)
contentY()       → 0 if status bar hidden, else 14   (reclaims top strip)
footerY(h)       → h - CHAR_H - 1       (bottom-anchored, computed from canvas height)
sep2Y(h)         → h - CHAR_H - 3
contentH(h)      → sep2Y(h) - contentY()
contentRows(h)   → contentH(h) / CHAR_H
cols(w)          → w / CHAR_W
```
The **top zone is fixed** (anchored to top edge); the **bottom zone (footer/separator)
is resolution-dependent** and must be computed from `canvas.height()`. Status bar can be
globally hidden (`statusBarVisible`, config `display/statusbar`), which reclaims the top
strip for content.

**Source files:** `node.h`, `layout.h`, `ui_constants.h`.

---

## 7. Component catalog (the design vocabulary)

Reusable builders in `widgets.h` (namespace `aether::ui::`). A production design should
be expressed using these — host-rendered consistency is guaranteed by reusing them.

### Layout primitives
`View`, `Row`, `Col`, `Container` (padded Col, flexGrow=1), `ScrollView`
(clips + scrollbar).

### Chrome
- **`TitleBar(title)`** — filled banner, inverted centred title (page title).
- **`Header(title)`** — title line + full-width separator.
- **`Footer(hint)`** — Caption-role bottom hint line (often paired with `sep2Y` divider).
- **`FooterLegends(items, count[, state])`** — soft-key bar of icon+label pills,
  count-driven space-between (1=left, 2+=edges); paper-colour content on filled
  capsules; optional wall-clock collapse-to-icon animation (`FooterLegendsState`).
  See [`feats/footer-legends.md`](../feats/footer-legends.md).
- **Status bar** — `StatusBar::draw(canvas, StatusBarData)`: clock, battery %, wifi/ble/
  sd/lock icons, version (`status_bar.h`). System-drawn, top of screen.

### Text
`Text(role)`, `SmartLabel` (Smart role: ellipsis/marquee), `Icon(bitmap, w, h)`.

### Lists (Flipper-style, Plan 79) — the dominant pattern in the system
- **`ListContainer(scrollState, rows)`** — ScrollView preset (Stretch, inset, gap),
  focuses rows with selection box + marquee.
- **`ListSection(title)`** — bold non-selectable subheader.
- **`ListItemRow(entry)`** — selectable row: `{label, value (right-aligned), leftIcon,
  chevron ">", onPress}`.
- **`ListInputRow(entry)`** — split `label  < value >` with Left/Right adjust.
- **`VirtualList(...)`** — windowed rendering for large lists (only visible items built);
  app drives focus by index via `VirtualListState::moveFocus(±1)` (`virtual_list.h`).

### Input controls (`widgets.h`)
- **`Toggle`** — `label   [ON]/[OFF]`.
- **`Stepper`** — `label   − value +`.
- **`Select`** — `label   < value >`.
- **`Slider`** — native track/fill/knob; Left/Right fine-adjust.
- **`TextField`** — `label: <text>` (inline display; editing via virtual keyboard overlay).
- **`Menu(items)`** — Col of Buttons.

### Modals & dialogs (Plan 70)
- **`Modal(children)`** — centred overlay box (white rect + border), ~¾×½ screen default.
- **`Dialog(title, body, icon, buttons[≤3])`** — full dialog; `DialogButton.danger`
  renders an inverted (destructive) button.
- **`Popup(text, icon)`** — notification, no buttons.
- **`Toast(message)`** — bottom notification, passes input through.

### Feedback / async
- **`SkeletonRow` / `SkeletonBlock`** — dashed loading placeholders (phase-animated).
- **`AnimatedIcon`** — draws current frame of an `AnimationPlayer`.

**Source files:** `firmware/core/include/nema/ui/widgets.h`, `virtual_list.h`,
`draw.h`, `status_bar.h`.

---

## 8. Iconography

- **Built-in pack:** `icon_pack.h` — `IconDef{handle, bitmap, w, h}`, mostly **8×8**
  1-bit XBM (row-major, MSB first; the footer `nav.*` icons are a compact **6×6**).
  Lookup by string handle via `findIcon(handle)`. Each `IconDef` carries its own
  `w`/`h`, so non-8×8 icons render fine — the renderer reads bits as `row*w+col`.
- **Handle namespaces:** `status.*` (wifi/bt/battery/charging), `feature.*`
  (apps/**wallet**/settings/gpio/subghz/nfc), `file.*` (folder/file/generic),
  `action.*` (warning/info/ok/spinner/dot), `nav.*` (up/enter — 6×6, footer legends).
- **App icons:** an app's `manifest.json` `"icon"` is either a built-in **handle** or a
  bundle-relative path. A `.papp` may ship `icon.raw` (4-byte header `[w u16][h u16]` +
  1-bit packed bitmap), loaded at install time and rendered in the launcher/app list.
- **Icon size tokens** (§4): status 8, inline 12, list-row 16, launcher grid 24, hero 32.

> **Open design question:** the built-in set is small and 8×8. A production launcher
> using `lg=24`/`xl=32` icon slots needs a coherent **icon family** authored at those
> sizes; today most icons are 8×8 and would scale poorly.

**Source files:** `firmware/core/include/nema/ui/icon_pack.h`, `app_manifest.h`.

---

## 9. Screen architecture & navigation

### IScreen modes (`screen.h` / `view_dispatcher.h`)
- **Normal** — status bar auto-drawn above a content area.
- **Fullscreen** — whole canvas, no status bar (Desktop, Lock, Dolphin, AppHost).
- **Modal** — previous screen + dither backdrop + centred floating dialog.

### Navigation (`ViewDispatcher`, Android-style stack)
`navigate(screen)` (push), `replace`, `goBack` (pop), `popTo`, `clearBackStack`,
`navigate(screen, Bundle args)`. Lifecycle: `onResume` / `onPause` / `onStop` /
`onBackPressed`. Modal backdrop uses `previous()` (second-from-top).

### Transitions (Plan 90 F4.1)
`Transition::{None, SlideLeft, SlideRight}`. Rendered by `AetherServer` as **two-pass
clip slide over 8 ticks** (no offscreen buffer): forward nav slides in from the right,
back gesture slides in from the left. Default is `None` (instant cut).

> **Design takeaway:** motion is currently **one effect** (horizontal slide) with a
> fixed 8-tick duration. A production motion language would define which transitions
> apply where (and whether to enable them at all on a slow 1-bit panel).

**Source files:** `view_dispatcher.h`, `component_screen.h`, `screen.h`.

---

## 10. Full screen map (the whole system)

```
Boot
 └─ DesktopScreen ............ Fullscreen idle/home; live wallpaper (IDesktopTheme skin)
     │                          [OK]→Launcher · [Back]→consumed (this is home)
     └─ LauncherScreen ....... System menu, 6 fixed entries; swappable ILauncherTheme
        │                       skins (Wii / Flipper / PlayStation / Compact); animated
        │                       T2 icons per entry. [Back]→Desktop
        ├─ Apps → AppListScreen ........ VirtualList of installed apps + 1-bit icons,
        │          │                      alphabetical. [Hold-OK]→AppDetail
        │          ├─ launch → AppHost (app on its own thread; Fullscreen blit)
        │          └─ AppDetailScreen ... per-app: permission toggles, storage (move
        │                                  Internal↔SD), uninstall
        ├─ Files → FileBrowserScreen ... Flipper-style VFS browser, icons + sizes + path
        │          ├─ TextViewerScreen .. monospace raw text viewer (scroll)
        │          └─ FileOpsModal ...... context menu (view/copy/cut/paste/rename/
        │                                  delete→confirm/new folder)
        ├─ Dolphin → DolphinDemoScreen .. Fullscreen .panim animation showcase
        ├─ Logs → LogsScreen ........... dense scrollable log viewer, level-tagged rows
        ├─ Settings → SettingsScreen ... capability-gated list; owns 15 child screens ↓
        └─ BadUSB → app (via AppRegistry)

SettingsScreen children:
  About · Sleep/Display · Appearances (→Desktop Setting) · WiFi (→Network Detail →IP
  Config) · Bluetooth · Remote · Controls · Touch · Sounds · Camera · Storage · Profile
  · Developer · (Apps in detail mode → AppDetail)

System-triggered overlays (not in the launcher tree):
  PermissionScreen (Modal, 210×100) — shown when an app requests a sensitive capability
  CloseAndOpenModal (Modal, 220×86)  — launch-while-one-paused single-slot prompt
  Virtual keyboard overlay           — text entry in WiFi/Profile/Remote/rename
  LockScreen (Fullscreen)            — clock + double-Activate to unlock
```

**The three "face of the product" screens** (highest branding weight):
**Desktop**, **Launcher**, **App List**. The **Settings/list pattern** (`ListContainer`
+ `ListItemRow`/`ListInputRow`) is the dominant interaction surface and the place where
visual consistency matters most across 15+ screens.

**Skin systems already in place:** `IDesktopTheme` (desktop wallpaper skin),
`ILauncherTheme` (launcher visual skin: Wii/Flipper/PlayStation/Compact), `StyleTokens`
(global tokens), font packs (typography skin). All are config-selectable
(`config "aether/desktop"`, `"aether/launcher"`, etc.). See
[`shell-desktop-launcher.md`](shell-desktop-launcher.md).

**Source files:** `firmware/core/include/nema/screens/*`,
`firmware/aether/include/aether/screens/{desktop_screen,launcher_screen}.h`.

---

## 11. App-facing UI surface (what third-party apps can render)

Installable apps (WASM `.papp`) cannot use the full widget set — they use a **constrained
retained-UI ABI**, which guarantees apps look consistent with the system. This bounds
what app screens in a production design can look like.

### Retained UI ABI (`packages/app-sdk/include/nema_api.h`, host: `wasm_ui.cpp`)
```c
ui_begin();                      // start frame, enter GUI mode
ui_title("..."); ui_text("...") // header: Subhead + Body text
ui_button("Label", id);          // focusable list row, id>0 returned on activate
ui_row_begin()/ui_row_end();     // horizontal flex container
ui_col_begin()/ui_col_end();     // vertical flex container
ui_end();                        // render frame
int ui_wait_event();             // block → button id | EV_BACK(-1)
int ui_poll_event();             // non-block → id | EV_BACK | EV_NONE(0)
```
The host builds the tree into a `ListContainer`-style layout with system fonts/focus.
**No host→guest callbacks** (invariant I7): the app owns its loop and polls.

### Raw canvas ABI (for custom-drawn apps)
`canvas_width/height`, `canvas_clear`, `canvas_pixel`, `canvas_fill_rect`,
`canvas_rect`, `canvas_line`, `canvas_text`, `canvas_flush`. Colour is 0/1 (invariant
I6). Apps must use `canvas_width()/height()`, never hardcode size (invariant I2).

> **Design takeaway:** system apps (C++ screens) get the full §7 catalog; **WASM apps
> get title + text + buttons + row/col + raw canvas**. A production design language for
> the app ecosystem should be authored against this smaller surface so third-party apps
> stay on-brand.

**Source files:** `packages/app-sdk/include/nema_api.h`,
`firmware/core/src/wasm/wasm_ui.cpp`, `examples/wifi-marauder/main.c` (reference app).

---

## 12. Footer hints & input affordances

Button labels are **never hardcoded** in screens — they come from the board's keymap so
the same UI is correct on a 6-button, 3-button, or 2-button board.

- `rt.input().hintFor(Action)` → board-specific label (e.g. `Back` → "Cancel" on 6-btn,
  "Hold ●" on 3-btn, "◀+▶" on 2-btn) — `i_key_map.h`, `input_service.h`.
- Screens program against **`input::Action`** (Prev/Next/Activate/Back/…), the
  hardware-agnostic intent layer — not raw keys.
- `Footer(rt.input().hintFor(Action::Back) + " to exit")` is the idiom.

> **Design rule:** any concept that shows on-screen button glyphs/labels must source them
> from `hintFor()`, because the physical control set varies per board.

**Source files:** `i_key_map.h`, `input_service.h`, `input_action.h`.

---

## 13. Animation system

- **Frame animation** — `Animation`/`AnimationFrame` (1-bit XBM sequences, frameRate,
  loop, optional non-linear order, Flipper passive/active frame split). Played by
  `AnimationPlayer`, ticked by `AnimationManager` in lockstep with the render loop
  (`animation.h`, `animation_player.h`). Used by launcher T2 icons, Dolphin, AnimatedIcon.
- **Spring physics** — `AnimatedValue{value,target,velocity,stiffness,damping}` for
  smooth scalar transitions (scroll, progress, slide offsets) — `animated_value.h`. Not
  for 1-bit opacity.
- **Scroll momentum** — flick velocity on `ScrollState`, decayed per tick
  (`component_runtime.h tickMomentum`).
- **Screen transitions** — see §9/§10 (8-tick horizontal slide).

> **Open design question:** motion is functional but ad-hoc. A production concept should
> decide a **motion language**: which animations exist, durations, easing, and whether
> they're enabled on hardware (1-bit panels can ghost/smear on fast motion).

---

## 14. Design constraints & invariants (the rulebook)

A "final" design **must** satisfy these or it cannot be implemented as-is:

1. **1-bit only.** No colour/greyscale/alpha. Hierarchy = inversion + borders + space + type weight/size.
2. **Resolution-independent.** Derive everything from `canvas.width()/height()` and tokens; never hardcode 240×320 or any pixel coordinate.
3. **Flex-only layout.** Nested single-axis rows/columns; no wrap/grid/percent.
4. **Token-driven.** Spacing/icon-size/font-scale come from `StyleTokens`; extend the token set rather than scattering magic numbers.
5. **Role-based type.** Use `TextRole`; large type is pixel-doubled today.
6. **Component reuse.** Express designs with the `widgets.h` catalog so rendering/focus/marquee behaviour is consistent and free.
7. **Board-agnostic affordances.** Button labels from `hintFor()`, intents via `input::Action`, capabilities via `rt.capabilities().has(...)` — never branch on board name.
8. **System-owned chrome.** Status bar, modal dither backdrop, transitions compose in a fixed order; a design styles them but doesn't reorder them.
9. **App UI is constrained.** Third-party (WASM) apps render via title/text/button/row/col + raw canvas only.
10. **Performance honesty.** Full-screen motion is two-pass clip rendering; dirty-region partial redraw is the norm. Keep motion small/region-bound.

---

## 15. Open design questions to resolve for the production concept

These are the genuine decisions a final-design discussion should settle (each is a real
gap in the current code, not a critique of intent).

> **Several of these are now being resolved** by a locked concept — a colour token system,
> dark mode, themeable selection styles, and display rotation. See **§16 — Target design
> (proposed)** below, [ADR 0012](../decisions/0012-color-token-system-display-capability.md),
> and [Plan 92](../plans/92-color-system-rotation.md).

1. **Token coverage.** Should `StyleTokens` grow to include row height, selection style,
   border weight, divider style, and motion timing — so the *entire* look is themeable?
2. **Icon family.** Author a coherent icon set at the `lg=24`/`xl=32` slots (today most
   are 8×8) for the launcher/home.
3. **Typography ceiling.** Native large fonts vs. pixel-doubling for headings/clocks.
4. **Default skin.** The shell ships 4 launcher skins + desktop skins; which is the
   *production default*, and is the brand expressed through a skin or through tokens?
5. **Motion language.** Which transitions/animations are on by default on hardware, and
   their timing/easing.
6. **Status bar identity.** Filled vs outlined (`invertedStatusBar`), what indicators
   show, and whether it's on by default.
7. **App-ecosystem look.** A style guide for WASM apps within the constrained ABI so the
   third-party experience stays on-brand.
8. **Density profile.** Which token theme (`default`/`compact`/`large`) is the shipping
   baseline for the 240×320 panel.

---

## 16. Target design (proposed) — colour system, dark mode, rotation

> **Status: proposed concept, not yet implemented.** Architecture locked in
> [ADR 0012](../decisions/0012-color-token-system-display-capability.md); phased delivery
> in [Plan 92](../plans/92-color-system-rotation.md). This section describes where the
> design language is *heading*; §1–§15 describe what exists *today*.

The shift: **themes become colour-first** (not size-first), while staying board-agnostic so
the same UI renders real colour on an RGB panel and a clean on/off image on the 1-bit
SkyRizz-E32.

### 16.1 Colour tokens (RGB565 + hybrid mono)
A theme gains a semantic `ColorTokens` set — `bg`, `fg`, `accent`, `accentFg`, `muted` —
each a `Color { uint16_t rgb565; uint8_t monoOverride; }`. Widgets stay semantic: a list
row is `fg`-on-`bg`; the selected row is `accentFg`-on-`accent`. One codebase →
white-on-black (mono), black-on-orange (Flipper), etc.

- **Internal space:** RGB565 (matches the panels + existing `blitRgb565()`).
- **Mono fallback:** *hybrid* — auto-luminance threshold by default; per-token
  `monoOverride` (force ON/OFF) when auto loses contrast. Keeps degradation
  designer-controlled and pixel-perfect.

### 16.2 Where it lives (capability-driven, not board-typed)
```
Theme (aether, board-agnostic) → ColorTokens (RGB565 + mono channel) + SelectionStyle
Canvas + renderer (nema core)  → Color type; resolves rgb565 or toMono() per capability
Display driver (board)         → declares monochrome|rgb565; final pixel write; rotation
Board profile                  → ONLY selects the driver (defines no colour)
```
The `bool on` Canvas overloads are retained (→ `fg`/`bg`) so the migration is incremental.

### 16.3 Dark mode
Each theme carries two `ColorTokens` sets (`light` + `dark`); a global toggle (config
`display/dark_mode`) selects one. Flipper light `{bg: orange, fg: black}` ↔ dark
`{bg: black, fg: orange}`. The mono channel resolves both correctly on 1-bit.

### 16.4 Selection styles (resolves §15.1)
`SelectionStyle = { Invert, FillRounded, DropShadow }` becomes a theme token. **DropShadow**
= Wii-style 1px (x=1,y=1) offset in `muted` blended into `bg` — **colour-only**; monochrome
displays fall back to `Invert` (today's `selectBox`).

### 16.5 Display rotation
`0/90/180/270` as a **driver** capability — HW `MADCTL` preferred, software `flush()`
fallback. Default from the board profile; runtime override persisted to `display/rotation`.
Because the UI is resolution-independent, rotation only swaps `canvas.width()/height()` and
the layout reflows. **Touch coordinates must apply the same transform** (the known trap).

### 16.6 Pixel-perfect chrome
Integer logical→physical scale only; design at native logical resolution; snap to spacing
tokens; hand-tuned bitmaps. Status-bar redesign: simpler battery glyph; clock moved
**top-left** in the smaller `Tiny` font.

---

## 17. Source-of-truth file index

| Concern | Header / file |
|---|---|
| Low-level draw API, fonts, clip | `firmware/core/include/nema/ui/canvas.h` |
| Higher-level draw helpers (shapes, scrollbar, marquee, banner) | `…/ui/draw.h` |
| Node tree (NodeType, Style, TextRole, ScrollState) | `…/ui/node.h` |
| Flexbox layout engine | `…/ui/layout.h` |
| Widget builders (catalog) | `…/ui/widgets.h` |
| Virtual list | `…/ui/virtual_list.h` |
| Component screen base / runtime | `…/ui/component_screen.h`, `…/ui/component_runtime.h` |
| Renderer | `…/ui/renderer.h` |
| Design tokens / themes | `…/ui/style_tokens.h` |
| Text roles → font handles | `…/ui/text_style.h` |
| Font registry & handles | `…/ui/font_registry.h` |
| Layout constants & zones | `…/ui/ui_constants.h` |
| Status bar | `…/ui/status_bar.h` |
| Icon pack | `…/ui/icon_pack.h` |
| Animation | `…/ui/animation.h`, `…/ui/animation_player.h`, `…/ui/animated_value.h` |
| Navigation / transitions | `…/ui/view_dispatcher.h`, `…/ui/screen.h` |
| Display server contract / Aether | `…/ui/display_server.h`, `…/ui/aether_server.h` |
| Input intents & hints | `…/input/i_key_map.h`, `…/input/input_action.h`, `input_service.h` |
| System screens | `firmware/core/include/nema/screens/*` |
| Shell (desktop/launcher) | `firmware/aether/include/aether/screens/*` |
| App UI ABI | `packages/app-sdk/include/nema_api.h`, `firmware/core/src/wasm/wasm_ui.cpp` |
| Reference app | `examples/wifi-marauder/main.c`, `examples/wifi-marauder/manifest.json` |

**Companion architecture docs:** [`aether-ui.md`](aether-ui.md),
[`font-system.md`](font-system.md), [`shell-desktop-launcher.md`](shell-desktop-launcher.md),
[`how-to-write-a-screen.md`](how-to-write-a-screen.md), [`ui-app-input.md`](ui-app-input.md),
[`virtual-keyboard.md`](virtual-keyboard.md), [`assets.md`](assets.md).
