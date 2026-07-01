# Plan 98 — P4: Dirty-Rect / Partial-Redraw Rendering (deep analysis)

**Status:** analysis (not started) · **Created:** 2026-07-01 · **Parent:** [Plan 97](97-app-runtime-responsiveness.md) (P4)

> This is a **research-first** document, written because P4 is rendering-critical and
> bug-prone. It is grounded in (a) actual codebase data with file:line, and (b)
> external prior art (LVGL, classic dirty-rect). It corrects an earlier hand-wave
> ("slow path is limited by compositing / ghosting") — the truth is more useful.

---

## 0. TL;DR — what the data actually says

1. **The dirty-rect machinery already exists in our renderer and is correct** — it is
   just **dormant**. Clip-aware clear, clip-respecting draw ops, a dirty bounding-box
   accumulator, and `getDirtyBounds → setClip` are all wired. They are almost never
   fed: `requestRedraw()` (full) has **118 call sites**; `requestRedraw(x,y,w,h)`
   (partial) has **~2**.
2. **Compositing is NOT the blocker.** The existing clip approach recomposites the
   full layer stack *within* the dirty rect (clip-aware `clear()` + redraw all layers
   clipped), which is exactly how LVGL does it — correct, no ghosting, *provided the
   dirty rect is accurate*.
3. **On skyrizz the SPI is already partial.** `LcdDriver::flushBuffer` row-diffs the
   frame against `prevBuf_` and only pushes changed rows. So for the common case
   (skyrizz, fullscreen app) the *bandwidth* win of P4 is already banked. What remains
   is **CPU redraw cost** (rebuild + layout + render the whole tree each frame).
4. **The real risk is under-reported dirty rects → stale pixels (ghosting).** Both
   LVGL and classic dirty-rect literature state the rule: the dirty rect must cover
   **every** changed pixel, including overdraw (borders, focus rings, shadows), or you
   must re-copy from the previous coherent buffer. Manual per-screen annotation across
   118 sites is where this risk lives.
5. **Our UI is rebuilt every frame** (`renderComponentFrame`, a `UiNode` tree from a
   `NodeArena`). That is the key enabler: dirty regions can be derived **automatically
   by diffing the rebuilt tree** — exact by construction, no manual annotation, no
   ghosting risk.

**Bottom line:** P4 done right = *automatic* dirty tracking (tree-diff for component
UI; auto-bbox for app buffers), not a 118-site manual conversion. And it should be
**gated on measurement** — see §5.

---

## 1. The render pipeline (codebase data)

### 1.1 Frame path
`GuiService::loop` (`core/src/services/gui_service.cpp:136`) each frame:
- drains input, ticks DPM/status/animations,
- if `vd.getDirtyBounds(dx,dy,dw,dh)` → `rt_.canvas().setClip(...)` (`gui_service.cpp:223`),
- `server->renderFrame(canvas, vd, status)`,
- `canvas.clearClip()`.

### 1.2 The compositor
`AetherServer::renderFrame` (`core/src/ui/aether_server.cpp:17`):
- `c.clear()` (line 97) — **clip-aware** (see §1.3),
- draws layers by screen mode: `StatusBar::draw` → background screen (for Modal) →
  dither backdrop + rounded modal box → `s->draw(c)` (the active screen),
- `c.flush()` (line 149) — **full-canvas** flush (no region variant).

So every redraw frame = clear + recomposite **all** layers + full flush. Within an
active clip, `clear` and every draw op are confined to the clip rect, so the composite
is correct inside the rect and untouched outside.

### 1.3 Clip + dirty primitives (already present)
- `Canvas::clear()` (`core/src/ui/canvas.cpp:14`): fast full path when no clip;
  otherwise `fillRect(clip)` — **clip-aware**.
- `Canvas::drawPixel/fillRect/...`: honor clip via `inClip()` / `clampToClip()`
  (`canvas.cpp:45,62`).
- `Canvas::setClip/clearClip/getClip` (`canvas.cpp:31`).
- `ViewDispatcher::requestRedraw(x,y,w,h)` (`core/src/ui/view_dispatcher.cpp:118`):
  **unions** the dirty box; `getDirtyBounds` returns it; `takeRedraw` consumes.
- `Canvas::flush()` (`canvas.cpp:250`) → `driver_.flush()` — **whole canvas**.

### 1.4 Usage reality (the gap)
- `requestRedraw()` full: **118** call sites (screens/apps/animations).
- `requestRedraw(x,y,w,h)` partial: **~2**.
→ The partial path is built but effectively unused; nearly every change triggers a
full-screen recomposite.

### 1.5 The app path
Apps draw into an offscreen `BufferDisplay` (Plan 97 P3b: 1bpp), then
`AppHost::present` (`core/src/app/app_host.cpp`) does `requestRedraw()` **(full)** +
wakes the GUI. `AppHost::draw` either takes the **fast path**
(`display_->flushBuffer(readyBuf_)` — skyrizz, driver row-diffs) or the **slow blit**
(per-pixel into the GUI canvas — simulator, non-fullscreen, modal-over-app).

### 1.6 The UI model (the enabler)
`ComponentRuntime` (`core/include/nema/ui/component_runtime.h:19`): "The caller owns
the `UiNode` tree (**rebuilt each frame** from a `NodeArena`)"; `renderComponentFrame`
does build→layout→focus→render. System screens (`ComponentScreen`) and component apps
(`ComponentApp`) share this. **React-like rebuild** → a tree diff is possible.

### 1.7 Measurement already exists
The FPS overlay (`aether_server.cpp:136`) prints `"%u d%u/f%u"` = **fps / draw-ms /
flush-ms** (`lastDrawMs_`, `lastFlushMs_`). This is the exact instrument to decide
whether P4 is worth it (§5).

---

## 2. External prior art (cited)

### 2.1 LVGL invalidate-area (matches our clip model)
Objects call `lv_obj_invalidate()` with their bounding box **including `ext_draw_size`**
(shadow/outline overdraw); areas are stored in `inv_areas[]`, **joined/merged**
(`inv_area_joined[]`), then only those areas are redrawn — recompositing overlapping
content within each area. With 2 draw buffers, LVGL "always draws only the dirty
areas." This is our approach, proven at scale.

### 2.2 Classic dirty-rectangle technique
Track changed regions each frame, render only those, blit in a burst. The compositor
recomposites layers within each dirty tile.

### 2.3 The ghosting rule (the one that bites)
"Ensure **every** pixel within the dirty rectangles is up to date. If you can't know
for certain the areas dirtied, copy from the previous fully-coherent back buffer before
rendering." i.e. **under-reporting the dirty rect leaves stale pixels**. Overdraw
(borders/focus rings/shadows) must be inside the rect.

Sources:
- LVGL rendering pipeline & invalidation — https://deepwiki.com/lvgl/lvgl/4.1-drawing-pipeline-and-image-processing
- LVGL redraw area constraints — https://docs.lvgl.io/master/details/main-modules/display/redraw_area.html
- LVGL display porting (2-buffer partial) — https://docs.lvgl.io/9.1/porting/display.html
- Dirty-rectangle system — https://blogs.scummvm.org/subr3v/2014/07/20/dirty-rectangle-system-pt-1/
- Dirty rects / ghosting (DXGI) — https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements

---

## 3. What P4 would actually buy (and where it wouldn't)

| Path | Who | Bandwidth today | CPU redraw today | P4 win |
|---|---|---|---|---|
| Fast flushBuffer | skyrizz fullscreen app | **already row-diffed** | app renders its own frame; blit skipped | small (bandwidth already banked) |
| Slow blit | simulator, non-fullscreen, modal-over-app | full canvas flush | **full per-pixel blit + full recomposite** | real CPU win if dirty rect known |
| Component screens | Settings/menus (skyrizz + all) | full flush (driver diffs on skyrizz) | **full rebuild+layout+render each redraw** | real CPU win via tree-diff clip |

So P4's value is **CPU-bound**, concentrated in *component screens* and the *slow blit
path* — not skyrizz fullscreen apps (already optimal). This is why its ROI looked low
in Plan 97; the analysis confirms it, but also finds an automatic, low-risk route.

---

## 4. Design options (ranked by risk/reward)

### Option A — Manual per-screen `requestRedraw(region)` ❌ not recommended
Convert the 118 full calls to region calls. Highest ghosting risk (every site can
under-report), highest churn. Rejected — superseded by B.

### Option B — Automatic tree-diff for component UI ✅ recommended core
Because the `UiNode` tree is rebuilt each frame, store a compact **render signature**
of last frame in the persistent `ComponentState` (per node: laid-out bbox + a content
hash of what it draws). Next frame, after layout, diff signatures: nodes that
appeared/disappeared/moved/changed-content contribute their (old ∪ new) bbox to the
dirty region; **join** them (LVGL-style). Feed that to `requestRedraw(region)`.
- **Exact by construction** → no manual annotation, no under-reporting → **no ghosting**.
- Must include overdraw in each node's bbox (focus ring, border, scrollbar) — the §2.3
  rule. The node already knows its style, so this is local and checkable.
- Covers *all* component screens + component apps at once.
- Cost: retain a small per-node signature array in `ComponentState`; a layout+hash pass.
  If the whole tree changed, fall back to full redraw (correct, no worse than today).

### Option C — Automatic dirty-bbox in `BufferDisplay` (app slow path) ✅ optional
`BufferDisplay` accumulates a dirty bbox automatically as the app draws
(drawPixel/fillRect/clear/invertRect expand it; `clear()` = full). `AppHost::present`
reports it via `requestRedraw(bbox)`; `AppHost::draw` clips the slow blit to it.
- Exact by construction (auto) → no ghosting.
- Helps only the **slow blit** path (skyrizz fullscreen already uses fast flushBuffer).
- Caveat: an app that `clear()`s + fully redraws each frame yields a full bbox → no win;
  only incremental-draw apps benefit. The driver-level row-diff (skyrizz) already
  catches "unchanged" for the fast path.

### Option D — Partial flush (`flushRegion`) ⏸️ marginal
Add a clipped flush so only the dirty rect hits the panel. skyrizz `flushBuffer`
already row-diffs, so the marginal SPI win is small; would mainly help boards without a
driver-level diff. Defer.

---

## 5. GATE: measure before building (do this first)

P4 is only worth its risk if per-frame **draw time** is actually a bottleneck. We
already print it.

1. Enable the FPS overlay (the `d<n>/f<n>` = draw-ms / flush-ms readout,
   `aether_server.cpp:138`).
2. Read `d` (draw-ms) on the busiest screens (a long list, an app animating) on
   **skyrizz hardware**.
   - `d` small (≲ a few ms, comfortably inside the 33 ms frame): P4 CPU savings are not
     worth the complexity — **stop, keep it deferred.**
   - `d` large (tens of ms, or you see dropped frames / the `fps` sags below ~30):
     P4 (Option B) is justified.
3. Also note `f` (flush-ms): if flush dominates and it's a non-diffing board, Option D
   moves up.

This turns "is P4 worth it?" from opinion into a number we already have.

---

## 6. Recommended plan (if the gate says go)

1. **B first** (component tree-diff → auto dirty region). Land behind a config/FPS
   toggle so it can be A/B'd against full-redraw, and so a diff bug degrades to
   "full redraw" not "corruption".
2. Validate visually: skyrizz + simulator, on scroll/focus-move/value-change/modal
   open+close, watching specifically for **ghosting** (stale pixels where the old UI
   was). The overdraw/focus-ring cases are the ones to hammer.
3. **C** if the slow-blit path (simulator, non-fullscreen) still shows high draw-ms.
4. **D** only for a future board without a driver-level diff.

### Guard rails (from the ghosting rule)
- Any uncertainty in a node's changed area → widen to full-node bbox incl. overdraw.
- Any structural tree change we can't cheaply diff → fall back to full redraw for that
  frame. Full redraw is always correct; partial is the optimization.
- Never ship a silent partial path without the full-redraw fallback + the A/B toggle.

---

## 7. Open questions for implementation
- Cheapest correct per-node content hash (position + size + text/value + style id?).
- Where the previous-frame signature lives (extend `ComponentState`) and its memory cost.
- How focus-ring / scrollbar / modal-dither overdraw is folded into each node's bbox.
- Interaction with Plan 90 F4.1 slide transitions (already full-clip two-pass — keep
  full redraw during a transition).
