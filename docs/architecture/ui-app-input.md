# UI, App Model & Input

> The hardware-agnostic presentation and interaction layer: a 1-bit `Canvas`, a
> navigable stack of screens and threaded apps, and physical buttons translated into
> board-independent intents.

## Purpose

This subsystem renders a 1-bit (monochrome) UI through a logical-pixel `Canvas`, manages
a stack of screens and threaded apps, and translates physical buttons into
hardware-independent navigation intents — so one `core/` codebase drives boards with
different resolutions, button counts, and gesture capabilities.

## Rendering pipeline

- **Color model — 1-bit monochrome.** Every `Canvas` primitive takes a `bool on`, not a
  color. `Canvas` (`ui/canvas.h`, `ui/canvas.cpp`) is a **stateless wrapper over an
  `IDisplayDriver&`** — it owns no framebuffer; the packed 1-bpp buffer lives in the driver.
  One escape hatch: `blitRgb565()` / `supportsRgb565()` (camera/video), where the screen sets
  `suppressCanvasFlush()` so the GUI loop won't clobber it.
- **Two-layer model: immediate at the screen boundary, retained underneath.** `IScreen`
  (`ui/screen.h`) has one pure virtual `draw(Canvas&)`. Component screens build a *retained*
  `UiNode` tree inside `draw()`, then: `layout(root, w, h, roleMetrics())` (two-pass flexbox)
  → `render(root, canvas, focused)` (the `aether::ui::draw` toolkit). Focus is drawn as an XOR
  `invertRect` (monochrome-native). ScrollViews set `canvas.setClip(...)`.
- **ViewDispatcher = stack + dispatch, NOT rendering** (`ui/view_dispatcher.*`). Owns a
  non-owning `vector<IScreen*>` and routes input/lifecycle; **GuiService owns the actual render
  loop**. Android-style API (Plan 70): `navigate(screen[, Bundle])` (push), `replace`, `goBack`,
  `popTo`; legacy `push/pop/popToRoot` forward to these. Dirty-rect via `atomic<bool>
  redrawPending_` (atomic because on WASM the app thread sets it from `present()` while the GUI
  thread polls): `requestRedraw()` / `requestRedraw(x,y,w,h)` / `takeRedraw()` / `getDirtyBounds()`.
- **Screen modes** (`ScreenMode{Normal,Fullscreen,Modal}`): Normal = status bar + content;
  Fullscreen = whole canvas; Modal = previous screen + backdrop + modal `draw()`.
- **Resolution independence.** Canvas exposes logical pixels via a float `scale_`:
  `width() = driver_.width()/scale_`. Layout always takes the available area as params (callers
  pass `canvas.width()/height()`); the flexbox engine has no Canvas/display dependency
  (host-unit-testable).
- **Text is role-driven** (`ui/text_style.*`): no `TextStyle` struct — `TextRole` →
  `fontForRole(role)` → `FontSpec{handle, scale}`. Roles: Title→Primary, Mono→Mono,
  Caption/Body→Secondary. Larger text = bitmap pixel-doubling. Layout-measure and render-paint
  call the same `fontForRole`, so sizes agree. Alignment is handled by flex layout, not text.

## App model

- **`IApp`** (`app/app.h`) is a foreground app on its own Nema thread; `run(AppContext&)` may
  block freely (download/compute) without freezing the GUI. Single blocking entry point — no
  onStart/present/onAction on `IApp` itself: `id()/name()/run(ctx)`, plus `runProcess(ProcessContext&)`
  (headless, Plan 54), `fullscreen()`, `stackBytes()`.
- **`AppContext`** (`app/app_context.h`) multiply-inherits `ISurface` (`canvas()`, `present()`,
  `nextInput()`, `waitInput()`) and `ProcessContext` (args/cwd/env/stdio, `requestExit`,
  `runtime()`). Apps reach all services via `ctx.runtime()`.
- **`AppHost`** (`app/app_host.*`) is the dual-threaded bridge: it inherits BOTH `IScreen` (GUI
  thread sees a screen) AND `AppContext` (app thread sees a surface). Shared state = the pixel
  buffer (guarded by `frameMtx_`) + a thread-safe input mailbox — no shared model, no race.
  - **Spawn** in `onResume()`: allocates `drawBuf_` (app renders here) + `readyBuf_` (latest
    presented frame), builds a `BufferDisplay`+`Canvas`, starts the app thread; a crash logs and
    posts `events::AppHostExited`.
  - **present()/handoff**: `present()` memcpys `drawBuf_`→`readyBuf_` under the mutex, bumps
    atomic `frameSeq_`, calls `view().requestRedraw()`. GUI `draw()` locks, records
    `drawnSeq_=frameSeq_`, blits `readyBuf_` (fullscreen fast path = `display_->flushBuffer(...)`).
  - **Reaper**: `tick()` (GUI thread) joins the finished thread and pops the screen; a
    `frameSeq_ != drawnSeq_` safety net forces a redraw so the first frame always paints
    (WASM race fix).
  - **Pause/resume** (Plan 22): `setPaused(true)` parks the app thread inside `waitInput()` at ~0 CPU.
- **Input flow** (`app_host.cpp`): GuiService dispatches `onAction(a)` then `onCode(c)` per
  press. AppHost buffers the action and emits ONE event on the code, preserving the true
  physical key (avoids the lossy `keyFromAction` round-trip). Don't "fix" this apparent
  asymmetry.
- **ComponentApp vs ComponentScreen** — same declarative runtime (flex layout, focus ring, tap,
  drag-scroll, flick), differing only in *where they run*:

  | | ComponentApp (`app/component_app.*`) | ComponentScreen (`ui/component_screen.*`) |
  |---|---|---|
  | Base / thread | `IApp` / own app thread | `IScreen` / cooperative on GUI thread |
  | Loop | owns build→layout→render→present + `waitInput()` | none; GuiService calls onAction/draw/tick |
  | Use | launchable apps that may block | built-in system screens |

  `ComponentApp` adds `build(arena, ctx)`, optional `buildModal()`, `onStart`, `onKey`,
  `onPointer`, `onTick`/`tickIntervalMs`, `capturesInput()`, `drawRaw()`.
- **capturesInput / modal**: `fullscreen()` decides compositing (status bar above app vs whole
  screen). `capturesInput()` false = focus navigation; true = all keys → `onKey()` (e.g. virtual
  keyboard), but the loop still `requestExit()`s if `onKey` rejects Cancel so it can't get stuck.
  A live `buildModal()` overlay captures input; Cancel dismisses the modal, not the app.
  `AppHostManager` enforces a single-app slot (launch-while-paused → `close_and_open_modal`).

## Input abstraction

Three tiers plus a gesture dimension, translated per-board by `IKeyMap`:

- **Key** (`ui/key.h`, ns `nema`) — legacy 6-button physical identity (Up/Down/Left/Right/Select/Cancel).
- **Code** (`input/input_code.h`, ns `nema::input`) — logical/geometric signal, may be absent
  per-board, extensible (`id ≥ 0x80` = custom): Up=1, Down=2, Left=3, Right=4, Enter=10,
  Escape=11, Menu=12.
- **Action** (`input/input_action.h`) — the intent layer apps target, split into a guaranteed
  **floor** (`Prev=1, Next=2, Activate=3, Back=4`) and optional (`AdjustUp/Down, Menu, Pause`).
- Inline converters bridge tiers: `codeFromKey`/`keyFromCode`, `defaultAction(Code)→Action`,
  `keyFromAction(Action)→Key`.

**IKeyMap** (`input/i_key_map.*`) is the single per-board translation layer (one per board), owning
a `GestureEngine`. There is no `translate()` method — the board's gesture callback
`(buttonId, Gesture) → (Code, Action)` feeds the protected `emitEvent(...)`, which derives
`key=keyFromCode(code)` and posts to `InputService`. Board feeds the engine via `feedEdge(...)`
+ `tick(...)`. **`validateFloor()`** (must pass at board init) requires all four floor actions
reachable. **`hintFor(Action)`** returns a board-specific label (e.g. Back → "Cancel" /
"Hold ●" / "◀+▶") so screens never hardcode button names (`rt.input().hintFor(Action)`).

**Gestures** (`input/gesture.*`): `{Short, Long, Double, Chord, Repeat, Hold}`; tunable timing
(longMs=500, repeatMs=150, holdMs=1000, doubleMs=280). Per-button modes: default, `setTwoStage`,
`setDoubleHold`. Reality vs stale comments: Double and Hold ARE implemented; **only Chord is
unimplemented**. Pointer/touch is separate (`input/pointer.h`): `PointerEvent{phase,x,y}` in
logical coords; `InputModality{Button,Pointer}` hides the focus ring during touch.

## Asset, font & animation system (Plan 71)

One bitmap format everywhere: raw 1-bit packed, row-major, MSB = leftmost, no header, drawn via
`Canvas::drawBitmap`. See [`../feats/asset-loader.md`](../feats/asset-loader.md).

- **asset_loader** (`ui/asset_loader.*`, ns `nema::asset`): `BitmapAsset` (one `.bm`, dims from
  filename `name_WxH.bm`); `AnimAsset` (a directory: `meta.txt` + `frame_N.bm`, wires an
  `anim::Animation` — **self-referential, must outlive its player, never copy/move after load**);
  `AssetPackLoader` (+ `AssetPackRegistry`) navigates a Flipper pack dir and bridges icons by
  handle; `seedDemoAssets(fs)` writes a demo pack to `/packs/default/`.
- **AssetArena** (`ui/asset_arena.cpp`): singleton bump allocator, 256 KB, PSRAM-backed on ESP32,
  O(1) `reset()` per screen transition.
- **Fonts**: `BitmapFont` (column-major, 1 byte/column). Generated by `tools/fonts/encode.py`:
  `FONT_PRIMARY` (5×8), `FONT_MONO` (6×8), `FONT_TINY` (4×6). **FontRegistry** (`ui/font_registry.*`)
  maps `FontHandle` → font with a `Secondary` fallback.
- **Animation** (`ui/animation.h`): `Animation{frames, frameRate, loop}` (const flash data);
  `AnimationPlayer` holds `const Animation&` + state, `tick(nowMs)` advances; `AnimationManager`
  (singleton) `tickAll(nowMs)` from the GUI loop. **GUI-thread only, not thread-safe** — every
  `registerPlayer` must be matched by `unregisterPlayer` before destruction. Apps on their own
  thread tick players manually.
- **Dolphin** (`ui/dolphin_anim.*`): `DOLPHIN_TV` (8 frames) + `dolphin_showcase.cpp` (10
  animations / 142 frames in `kDolphinBlob`). WASM dead-code elimination is countered with
  `__attribute__((used))`, `extern const`, and a `(void)DOLPHIN_SHOWCASE;` anchor.
- **icon_pack** (`ui/icon_pack.*`, Plan 53): 16 hard-coded 8×8 icons; `findIcon()` linear scan.

## Screens & built-in apps

**Screens** (`firmware/core/src/screens/`): home_screen (DSi-style carousel launcher),
file_browser_screen (VFS file manager), dolphin_demo (fullscreen showcase), app_list_screen,
settings_screen, sleep_settings_screen (actually the *Display* settings), controls_screen (input
diagnostics), logs_screen (ring-buffer viewer), about_screen, lock_screen, profile_settings_screen,
sounds_settings_screen, camera_settings_screen, touch_settings_screen, developer_screen,
close_and_open_modal (single-app-slot confirm).

**Apps** (`firmware/core/src/apps/`): dolphin_app (`ComponentApp` runtime-asset demo),
bad_usb_app (Ducky-script runner over `IUsbHid`) + badusb_parser, hello_app (widget demo),
js_app (QuickJS UI app) + js_app_store (process-wide owner) + embedded_apps.h (generated built-in
JS app sources). See [`scripting-and-apps.md`](scripting-and-apps.md).

## Conventions & gotchas

- **ViewDispatcher does not render**; **Canvas owns no framebuffer**.
- **AppHost is one object, two threads** — `redrawPending_` atomic + `frameSeq_!=drawnSeq_`
  safety net exist to fix WASM cross-thread blank-frame races.
- **AnimationManager is GUI-thread-only**; **AnimAsset must outlive its player and never be
  copied/moved** (dolphin_app `reserve()`s its vector to avoid reallocation).
- **Gesture comments are stale** — only Chord is unimplemented.
- **Doc/impl drift to watch**: AssetArena is wired but loaders still use `std::vector` heap;
  `FONT_PRIMARY/MONO/TINY` are compiled but the registry is still populated with `FONT_5X8`/
  `FONT_6X8` in `gui_service.cpp`.
- **Filename surprise**: `sleep_settings_screen` is the Display settings screen.

## Key files

| Area | Files |
|---|---|
| Canvas / renderer / layout | `firmware/core/{include/nema,src}/ui/canvas.*`, `renderer.*`, `draw.*`, `layout.*` |
| ViewDispatcher / Screen | `firmware/core/{include/nema,src}/ui/view_dispatcher.*`, `screen.h`, `surface.h` |
| Text / fonts | `firmware/core/{include/nema,src}/ui/text_style.*`, `font_registry.*`, `font_{primary,mono,tiny,5x8,6x8}.cpp` |
| App model | `firmware/core/include/nema/app/{app,app_context,app_host,component_app}.h` + `src/app/*` |
| ComponentScreen | `firmware/core/{include/nema/ui,src/ui}/component_screen.*` |
| Input | `firmware/core/include/nema/input/{input_action,input_code,i_key_map,gesture,pointer}.h` + `src/input/*`, `ui/key.h` |
| Assets / animation | `firmware/core/{include/nema,src}/ui/{asset_loader,asset_arena,animation,animation_player,animation_manager,dolphin_anim,icon_pack}.*` |
| Screens / apps | `firmware/core/src/screens/*`, `firmware/core/src/apps/*` |
