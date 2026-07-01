# 0020 — Unified diffing flush (`pushMono`) + scaled fast-path

**Status:** accepted · **Date:** 2026-07-01 · **Plan:** [98](../plans/98-dirty-rect-rendering.md)

## Context

FPS-overlay measurements on skyrizz hardware (2026-07-01) showed **flush time
`f≈48 ms` flat on every screen**, dominating component screens (Settings `d14/f48`).
Root cause: the row-diff added in Plan 97 P3b lived only in `LcdDriver::flushBuffer`
(the fullscreen-app fast path). Component screens draw into `framebuf_` then call
`LcdDriver::flush()`, which pushed the **whole** panel unconditionally — no diff — so a
single-character change cost a full ~48 ms SPI push.

Separately, apps at the skyrizz default **2× UI scale** render into a *logical*
(half-size) buffer, so the unscaled `flushBuffer` fast path (which requires
`w==width_`) was skipped and every fullscreen app fell to the per-pixel canvas blit —
the measured `d≈92 ms`.

## Decision

**1. Unify `flush()` and `flushBuffer()` into one diffing engine `pushMono(buf)`.**
Both diff a 1-bit packed frame against `prevBuf_` (the single "what's on the glass"
buffer) and send only changed rows. `flush()` passes `framebuf_`; `flushBuffer()`
passes the app buffer. Exactly one runs per frame, so `prevBuf_` has a single writer
per frame and stays coherent across the two callers.

**2. Add a scaled fast-path.** `IDisplayDriver::flushBufferScaled(buf, lw, lh, scale)`
(default returns false → caller keeps the canvas blit). `AppHost::draw` uses it when a
fullscreen app's logical buffer tiles the panel by an integer scale; skyrizz expands
the logical 1-bit buffer to the panel in one SPI pass. `suppressCanvasFlush()` became a
per-frame `directFlushed_` flag set by `draw()` (it must reflect whether a direct push
actually happened, incl. the driver-returns-false fallback).

## Why this is safe (the subtle part)

A pixel-exact **row diff cannot cause ghosting** the way a reported dirty *rectangle*
can — it compares actual bytes, so it never under-reports. The only failure mode is
`prevBuf_` going out of sync with the glass. Every path that writes the panel outside
`pushMono` must therefore force a full repaint:
- **`setPalette` on colour change** — the 1-bit content is identical but the fg/bg
  colours differ, so the diff would see "no change" and leave stale colours. Guarded:
  `setPalette` sets `fullFlush_` only when fg/bg actually change (it is called every
  frame; unchanged calls must stay cheap so the diff keeps working).
- **`blitRgb565` (camera)** — writes RGB565 straight to the panel; sets `fullFlush_`.
- **init / `setRotation`** — already set `fullFlush_`.
- **`flushBufferScaled`** — full push, no diff (scaled apps are usually
  fullscreen-animating); sets `fullFlush_` so the next component `flush()` repaints.

Also fixed a latent P3b bug found here: the **default** `IDisplayDriver::flushBuffer`
still read the buffer as 1 byte/pixel, but P3b made those buffers 1bpp packed. Now it
reads `mono1`, so boards that don't override it (async/dev-board) don't corrupt.

## Consequences

- **+** Component screens: `f 48 ms → ~0` when unchanged; small changes flush only the
  touched rows. Static/menu screens should hit the 30 fps cap.
- **+** Fullscreen apps at 2×: the `d≈92 ms` blit becomes a single scaled SPI push.
- **+** One flush engine instead of two divergent ones; `prevBuf_` is the single panel
  mirror.
- **−** New invariant: **any new panel-writing path must set `fullFlush_`** or it will
  ghost under the diff. Documented in `pushMono`'s comment and here.
- **−** `flushBufferScaled` does no diff yet (full push each call). Fine for animating
  apps; a future logical-level diff could make small-change scaled apps cheap too.
- Board-specific: only skyrizz implements the overrides; other boards fall back
  (default `flushBufferScaled` = false; their `flush`/`flushBuffer` unchanged).
- Not yet ESP32-compiled (parallel http WIP blocks the build) or visually verified on
  glass — see Plan 98 validation checklist.
