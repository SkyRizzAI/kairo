# 29 — Touch HAL & Dual-Modal Components

> Touch menjadi HAL capability sejajar dengan display & buttons. Board yang
> punya touch controller meng-expose `ITouchDriver`; komponen UI (Pressable,
> ScrollView) menerima `PointerEvent` dalam koordinat **logical** dan bereaksi
> sama seperti input tombol. Model interaksi meniru web (`:focus-visible`):
> focus-ring hanya muncul saat navigasi tombol, hilang saat layar disentuh.

- Status: ☐ Not started
- Milestone: M8 (Hardware Portability)
- Depends on: 14 (UI Runtime), 25 (Adaptive UI / scale), 27 (Input Abstraction), 28 (SkyRizz E32 board)
- Blocks: 30 (Component runtime + screen migration), 31 (ScrollView/gesture)

---

## Latar belakang

Kairo sudah punya component system lengkap (`widgets.h`, `layout.h`, `focus.h`,
`renderer.h`) yang dipakai **100% oleh apps** via `ComponentApp`. Setiap
`Pressable` setelah `layout()` punya **hit-rect (x/y/w/h)** + **`onPress`**.

Itu = titik penyatuan alami untuk dua modalitas:
- **Tombol** → focus traversal → Activate → `onPress`
- **Touch** → hit-test (x,y) → Pressable yang kena → `onPress` yang **sama**

Touch karena itu **aditif**, bukan rewrite: cukup tambah event pointer + hit-test
di atas rect yang sudah dihitung layout.

### Filosofi (paralel dengan Plan 27)

| Modalitas | Lapisan per-board (serap keanehan HW) | Output kanonik | Konsumen |
|---|---|---|---|
| Tombol | `IKeyMap` | `input::Action` | komponen |
| **Touch** | **`ITouchDriver`** | **`PointerEvent` (koordinat logical)** | komponen |
| Display | `IDisplayDriver` | pixel logical | Canvas |

Sama seperti `IKeyMap` menyerap short/long/chord, **`ITouchDriver` menyerap
protokol chip + orientasi + kalibrasi**. Yang keluar dari driver sudah dalam
koordinat logical (ikut rotasi MADCTL + scale Plan 25), sehingga komponen 100%
board-agnostic.

---

## Arsitektur

```
Touch controller (FT6336U / TSC2007 / …)
   │  chip protocol (I2C), IRQ
   ▼
[ITouchDriver per-board]   ← debounce + transform raw → LOGICAL coords
   │  emit: PointerEvent { phase, x, y }
   ▼
[InputService — single funnel]   ← carries Action AND Pointer
   │
   ▼
[Component runtime]   (ComponentApp loop now; extracted in Plan 30)
   │  • Action  → focus traversal → onPress
   │  • Pointer → hit-test → onPress
   │  • tracks InputModality (Button / Pointer)
   ▼
[renderer]  → focus-ring HANYA saat modality == Button (web :focus-visible)
```

### 1. `PointerEvent` (core)

```cpp
namespace kairo::input {

struct PointerEvent {
    enum class Phase : uint8_t { Down, Move, Up };
    Phase    phase = Phase::Down;
    uint16_t x = 0;   // LOGICAL canvas coordinates (already scaled/rotated)
    uint16_t y = 0;
    // Single-touch for v1. Multi-touch id reserved for later.
};

enum class InputModality : uint8_t { Button, Pointer };

} // namespace kairo::input
```

### 2. `ITouchDriver` (core HAL)

```cpp
namespace kairo {

struct ITouchDriver : IDriver {
    DriverKind kind() const override { return DriverKind::Other; }

    // Poll/IRQ-driven internally; the driver pushes PointerEvents into the
    // InputService it was given. Coordinates MUST be logical (post-transform).
    virtual void start() = 0;
    virtual void stop()  = 0;

    // Called from the board to wire the funnel (mirrors IKeyMap::attachInput).
    void attachInput(InputService* svc) { input_ = svc; }

protected:
    void emitPointer(input::PointerEvent::Phase ph, uint16_t x, uint16_t y);
    InputService* input_ = nullptr;
};

} // namespace kairo
```

The driver is responsible for ALL board-specific concerns:
- chip I2C protocol + IRQ handling (or polling)
- debounce / deglitch
- **coordinate transform**: raw panel coords → logical display coords
  (rotation to match MADCTL, scale from Plan 25, axis flip, and — for resistive
  panels — calibration). Capacitive (FT6336U) gives direct pixel coords; the
  transform is mostly orientation. Resistive (TSC2007) adds 4-point calibration
  — entirely the driver's internal concern, invisible to components.

### 3. InputService — single funnel for both

```cpp
class InputService {
public:
    // existing: post(Key)/post(InputEvent)/next(InputEvent&)  — Action/Key path

    // NEW — pointer path (same service, one funnel).
    void postPointer(const input::PointerEvent& e);
    bool nextPointer(input::PointerEvent& out);

    // Consecutive Move events are coalesced so a drag can't flood the queue.
    // ...
};
```

The component runtime drains both `next()` (actions) and `nextPointer()` each
frame. Whichever fired last sets the current `InputModality`.

### 4. Hit-testing + dispatch (in the component loop)

```cpp
// Walk the laid-out tree, return the TOP-MOST focusable node whose rect
// contains (x,y). Tree order = paint order; last match wins (topmost).
UiNode* hitTest(UiNode& root, uint16_t x, uint16_t y);

// Pointer dispatch:
//   Down → record pressedNode (for :active feedback), set modality=Pointer
//   Up   → if release is over the same node → fire node->onPress(userdata)
//   Move → (Plan 31: drag/scroll)
bool handlePointer(UiNode& root, FocusState& fs, const PointerEvent& ev,
                   UiNode*& pressedNode, InputModality& mode);
```

Wiring into `ComponentApp::run()` (apps get touch immediately):
```cpp
// drain actions (existing handleFocusKey path) ...
input::PointerEvent pe;
while (ctx.input().nextPointer(pe)) {
    mode_ = InputModality::Pointer;
    if (handlePointer(*root, fs, pe, pressed_, mode_)) dirty = true;
}
```

### 5. Modality-aware focus (web `:focus-visible`)

- `renderer::render()` gains the current modality; draws the focus highlight
  (`invertRect`) **only when `modality == Button`**.
- On `PointerEvent::Down`, modality flips to `Pointer` → ring disappears; the
  tapped node gets transient pressed feedback (`:active`) instead.
- Any subsequent button press flips modality back to `Button` → ring returns on
  the focused node.

---

## SkyRizz E32 — FT6336U touch driver

> ⚠️ Pin doc menyebut TSC2007, tetapi hardware nyata = **FT6336U kapasitif**
> (dikonfirmasi referensi Rust). Kapasitif → koordinat pixel langsung, tanpa
> kalibrasi resistif.

| Param | Nilai |
|---|---|
| Controller | FT6336U (capacitive) |
| I2C address | `0x38` |
| PENIRQ | `GPIO2` (active-LOW) |
| Reset | XL9535 `P01` (sudah ada `Xl9535::setTouchReset()`) |
| Native panel | 240×320 portrait, MADCTL `0x48` |

Reset sequence (dari Rust ref): TS_RST LOW 50ms → HIGH → tunggu 400ms.

Register:
- `0x02` TD_STATUS → jumlah titik (`& 0x0F`)
- `0x03..0x06` TOUCH1 XH/XL/YH/YL → `x = ((XH&0x0F)<<8)|XL`, `y = ((YH&0x0F)<<8)|YL`

Transform raw → logical: panel native sudah 240×320 portrait yang cocok dengan
orientasi LCD MADCTL `0x48`, jadi transform-nya minimal (clamp + scale Plan 25).
Jika ternyata sumbu kebalik/mirror saat bring-up, koreksi flip di driver.

```cpp
namespace kairo::skyrizze32 {
class Ft6336Touch : public ITouchDriver {
public:
    void init(Runtime& rt, Xl9535& expander);
    const char* name() const override { return "Ft6336Touch"; }
    void start() override;   // reset pulse via XL9535 P01 + IRQ/poll setup
    void stop()  override;
    void tick(uint64_t nowMs) override;   // poll TD_STATUS, emit PointerEvent

private:
    PointerEvent::Phase trackPhase(uint8_t points);  // down/move/up edge logic
    void toLogical(uint16_t rawX, uint16_t rawY, uint16_t& lx, uint16_t& ly);
    // ...
};
}
```

Board wiring (`skyrizz_e32.cpp`):
```cpp
touch_.init(rt, expander_);
touch_.attachInput(&rt.input());
rt.container().registerService(&touch_);
rt.hardware().add({"touch", DriverKind::Other, "FT6336U capacitive"});
rt.capabilities().add("input.touch");
```

---

## File structure

```
firmware/core/
├─ include/kairo/input/
│  ├─ pointer_event.h        # PointerEvent, InputModality
│  └─ i_touch_driver.h       # ITouchDriver HAL
├─ src/input/
│  └─ i_touch_driver.cpp     # emitPointer()
├─ include/kairo/services/input_service.h   ← + postPointer/nextPointer
├─ src/services/input_service.cpp
├─ include/kairo/ui/hit_test.h              # hitTest(), handlePointer()
├─ src/ui/hit_test.cpp
├─ src/ui/renderer.cpp        ← render() takes InputModality (ring only on Button)
├─ src/ui/focus.h/.cpp        ← FocusState keeps modality-agnostic
└─ src/app/component_app.cpp  ← drain pointer + dispatch + modality

firmware/boards/skyrizz-e32/
├─ include/kairo/skyrizze32/ft6336_touch.h
└─ src/ft6336_touch.cpp
```

---

## Tasks

- [ ] `pointer_event.h` — `PointerEvent` + `InputModality`
- [ ] `i_touch_driver.h/.cpp` — `ITouchDriver` HAL + `emitPointer()`
- [ ] `InputService` — `postPointer()`/`nextPointer()` (+ Move coalescing)
- [ ] `hit_test.h/.cpp` — `hitTest()` (topmost focusable at point) + `handlePointer()`
- [ ] `renderer.cpp` — accept `InputModality`; focus-ring only when `Button`; pressed feedback on touch
- [ ] `component_app.cpp` — drain pointer events, hit-test dispatch, track modality
- [ ] SkyRizz `Ft6336Touch` — reset via XL9535 P01, read TD_STATUS/XY, raw→logical, emit
- [ ] SkyRizz board — register touch service + `input.touch` capability
- [ ] Verify: tap a Button in an app (e.g. Counter) → its `onPress` fires; focus-ring gone while touching, returns on button press

## Acceptance criteria

- [ ] `capabilities().has("input.touch")` → true on SkyRizz, false on dev-board
- [ ] In a ComponentApp, tapping a `Pressable` fires the same `onPress` as button-Select
- [ ] Focus-ring is hidden after a touch and reappears after any button press (`:focus-visible` behavior)
- [ ] Touch coordinates land on the correct on-screen element (transform correct for 240×320 portrait)
- [ ] Drag does not flood the input queue (Move coalescing works)
- [ ] dev-board (no touch) unaffected — builds and runs identically

## Non-Goals (v1)

- Multi-touch / gestures (swipe, pinch, momentum) — Plan 31
- ScrollView / scrollbar — Plan 31
- Screen migration — Plan 30 (this plan makes touch work in **apps** first)
- Hover state (touch has none), event bubbling/capture, CSS-like cascade
