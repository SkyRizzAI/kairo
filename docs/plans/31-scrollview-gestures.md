# 31 — ScrollView, Scrollbar & Gestures

> **STATUS (2026-06-05): IMPLEMENTED.** `NodeType::Scroll` + `ScrollState` are
> first-class in the flex layout: a Scroll node reports a BOUNDED main size to its
> flex parent (claims the viewport via flexGrow) while measuring children at their
> natural length — content beyond the viewport overflows and scrolls (the
> flex⇄overflow contract, like RN `flex:1 + overflow:scroll`). `ScrollView()`
> builder; `Canvas` gained a clip region; the renderer clips content to the
> viewport and draws a proportional scrollbar. Gestures live in `ComponentRuntime`:
> tap-vs-drag discrimination (6px threshold), touch drag-scroll, flick momentum
> (friction decay), and button-mode auto-scroll keeps the focused row visible.
> Adopted in AppList, Logs, About, Controls, Settings + the **ScrollDemo** example
> app (Settings → Scroll Demo). Host layout test covers the bounded-viewport +
> clamp behaviour. Not done: swipe-to-page, long-press context, virtual-keyboard
> key Pressables.

> Komponen `ScrollView` dwi-modal: di mode tombol auto-scroll menjaga item
> ter-focus tetap terlihat; di mode touch bisa di-drag/swipe dengan scrollbar
> indikator + momentum. Plus gesture recognizer (tap-vs-drag, swipe, long-press)
> di atas `PointerEvent`.

- Status: ☐ Not started
- Milestone: M8 (Hardware Portability)
- Depends on: 29 (Touch HAL), 30 (Component runtime + screens)

---

## Motivasi

List panjang (AppList, Logs, virtual keyboard) butuh scroll. Dua modalitas
berbeda kebutuhannya:

| | Tombol | Touch |
|---|---|---|
| Scroll | auto: jaga item ter-focus terlihat | manual: drag/swipe konten |
| Indikator | highlight item ter-focus | scrollbar + posisi |
| Momentum | tidak perlu | flick → inersia |

Satu komponen `ScrollView` menyembunyikan perbedaan ini — caller cukup bungkus
konten panjang dengannya.

---

## ScrollView

```cpp
// widgets.h
UiNode* ScrollView(NodeArena& a, Style s, std::initializer_list<UiNode*> children);
// Internally: a clipping viewport + a tall content column + scrollOffset.
```

- **Layout**: ukur konten penuh; viewport = tinggi yang diberi parent; simpan
  `contentH`, `viewportH`, `scrollY` (state per node, disimpan via stable id).
- **Render**: clip ke viewport; geser child sebesar `-scrollY`; gambar scrollbar
  di tepi kanan saat `contentH > viewportH` (thumb proporsional). Scrollbar
  tampil terus di touch, fade/optional di button mode.
- **Button mode**: saat focus pindah ke item di luar viewport, `scrollY`
  disesuaikan agar item terlihat (auto-scroll, seperti perilaku `scroll_` lama
  tapi sekarang generik).
- **Touch mode**: `PointerEvent::Move` (Plan 29) → ubah `scrollY` sebesar delta;
  `Up` dengan kecepatan tinggi → momentum.

## Gesture recognizer

State machine kecil di component runtime yang mengubah aliran `PointerEvent`
mentah → gesture tingkat tinggi, sebelum dispatch:

| Gesture | Definisi | Aksi |
|---|---|---|
| **Tap** | Down→Up, gerak < threshold (≈8px), < ~300ms | `onPress` (sudah di Plan 29) |
| **Drag** | Down→Move melewati threshold | scroll konten (ScrollView) |
| **Swipe** | Drag cepat lalu Up | scroll + momentum / page |
| **Long-press** | Down ditahan > ~500ms tanpa gerak | context action (opsional) |

Tap-vs-drag discrimination penting: begitu gerak > threshold, batalkan calon-tap
(jangan fire `onPress`), alihkan ke drag.

## Momentum / inertia

Saat swipe dilepas dengan kecepatan v, `scrollY` terus bergerak dengan
peluruhan (friction) per tick sampai berhenti atau kena batas (clamp di
0..contentH-viewportH). Implementasi: simpan velocity, decay di `tick()` runtime.

---

## Bonus: virtual keyboard touch-tappable

`VirtualKeyboard` (sudah ada, 2D grid) begitu dwi-modal otomatis bisa di-tap per
tombol — tiap key jadi hit-rect. Ini peningkatan UX besar untuk entri teks
(WiFi password dll) di SkyRizz. Bisa dikerjakan sebagai langkah akhir plan ini:
bungkus grid key sebagai Pressable sehingga tap huruf = ketik huruf, sejajar
dengan navigasi tombol yang sudah ada.

---

## File structure

```
firmware/core/
├─ include/palanu/ui/widgets.h        ← + ScrollView builder
├─ src/ui/widgets.cpp
├─ src/ui/layout.cpp                 ← clip/viewport sizing for ScrollView
├─ src/ui/renderer.cpp               ← clip region + scrollbar draw
├─ include/palanu/ui/gesture_recognizer.h
├─ src/ui/gesture_recognizer.cpp     ← tap/drag/swipe/long-press over PointerEvent
└─ src/ui/component_runtime.cpp      ← feed pointer through recognizer; momentum tick
```

---

## Tasks

- [ ] `ScrollView` builder + node type (viewport + scrollY state)
- [ ] Layout: measure content, clamp scroll, viewport sizing
- [ ] Renderer: clip to viewport + draw proportional scrollbar thumb
- [ ] Button mode: auto-scroll to keep focused node visible
- [ ] Touch mode: drag updates scrollY
- [ ] `gesture_recognizer` — tap / drag / swipe / long-press over PointerEvent
- [ ] Momentum/inertia decay in runtime tick with bounds clamp
- [ ] Adopt ScrollView in AppListScreen + LogsScreen (replace interim clip)
- [ ] Virtual keyboard keys → Pressable (touch-tappable) while keeping 2D button nav
- [ ] Verify on SkyRizz: drag-scroll a long app list; flick → momentum; scrollbar tracks

## Acceptance criteria

- [ ] `ScrollView` scrolls via drag (touch) AND auto-scrolls to keep focus visible (buttons)
- [ ] Scrollbar thumb size/position reflects contentH/viewportH/scrollY
- [ ] Tap vs drag correctly distinguished (a small move during a tap still fires onPress; a large move scrolls and suppresses onPress)
- [ ] Swipe produces momentum that clamps at content bounds (no overscroll past limits)
- [ ] AppList + Logs scroll smoothly on SkyRizz touch and remain button-navigable on dev-board
- [ ] Virtual keyboard: tapping a letter types it; button grid nav still works

## Non-Goals

- Pinch-zoom / rotate gestures
- Nested scroll containers
- Overscroll bounce animation (just clamp)
- Horizontal scroll (vertical only for v1)
