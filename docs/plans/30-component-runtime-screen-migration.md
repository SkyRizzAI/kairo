# 30 — Component Runtime & Screen Migration

> **STATUS (2026-06-05): IMPLEMENTED (core + main screens).** `ComponentRuntime`
> extracted (`ui/component_runtime.{h,cpp}`) — shared build→layout→auto-scroll-to-
> focus→render + gesture pointer + nav + momentum. `ComponentApp` refactored to use
> it. New `ComponentScreen` base (`ui/component_screen.{h,cpp}`) wires
> draw/onAction/onPointer/tick through the runtime; screens implement
> `build(NodeArena&, Runtime&)`.
> **Migrated (ALL 11 screens):** Home, Settings, AppList, Logs, About, Controls,
> TouchSettings, SleepSettings, SoundsSettings, CameraSettings, LockScreen
> (fullscreen via `ComponentScreen::fullscreen()`). Only CameraApp stays legacy
> (real-time camera blit). Native input widgets added (Toggle, Stepper, Select,
> Slider, TextField) + Left/Right `dispatchAdjust`. The ScrollDemo moved out of
> Settings into a dedicated **UI Showcase** app in the Apps list (a plugin) with
> internal pages (Menu → Scroll List / Input Controls). Builds green on
> simulator, skyrizz-e32 and dev-board; host layout tests pass.

> Ekstrak loop component (build → layout → focus → render → input/hit-test) dari
> `ComponentApp` menjadi **ComponentRuntime bersama**, lalu migrasi SEMUA screen
> (Home, Settings, AppList, Logs, About, SleepSettings, Lock, Controls) dari
> imperative Canvas drawing ke `build()`-style component tree. Hasilnya: satu
> mesin komponen dipakai apps & screens, dan touch (Plan 29) jalan menyeluruh —
> bukan cuma di apps.

- Status: ☐ Not started
- Milestone: M8 (Hardware Portability)
- Depends on: 29 (Touch HAL & dual-modal core)
- Blocks: 31 (ScrollView — screens butuh ini untuk list panjang)

---

## Masalah

Audit (Plan 27 era) menemukan: component system dipakai **100% apps**, **0%
screens**. Semua 8 screen masih imperative — `canvas.drawText/fillRect/invertRect`
langsung + `int cursor_` manual + `update(Key)`.

Akibatnya touch (Plan 29) **hanya jalan di apps**. Agar Home/Settings/dll bisa
disentuh, screen harus pakai component tree (yang punya hit-rect). Hand-coding
hit-test di tiap screen = persis fragmentasi yang dihindari component system.

> Mengadopsi touch = forcing function untuk melunasi utang imperative screens.

---

## Bagian A — ComponentRuntime extraction

Saat ini loop hidup di `ComponentApp::run()` (threaded, via `AppHost`). Screens
jalan di GUI thread via `ViewDispatcher`. Kita **tidak** mau bikin tiap screen
jadi thread. Solusi: ekstrak loop jadi helper yang dipakai keduanya.

```cpp
namespace kairo::ui {

// Stateless-ish driver of one component frame + input dispatch.
// Owned by the caller (AppHost for apps; ViewDispatcher for screens).
class ComponentRuntime {
public:
    // Build → layout → focus → render one frame into canvas.
    // origin/size let the caller carve out the content area (below status bar).
    void renderFrame(UiNode* root, Canvas& c,
                     uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    // Route one action; returns true if a redraw is needed.
    bool dispatchAction(UiNode* root, input::Action a);

    // Route one pointer event (hit-test → onPress); sets modality=Pointer.
    bool dispatchPointer(UiNode* root, const input::PointerEvent& e);

    InputModality modality() const { return modality_; }

private:
    FocusState    fs_{};
    UiNode*       pressed_ = nullptr;
    InputModality modality_ = InputModality::Button;
};

} // namespace kairo::ui
```

- `ComponentApp::run()` refactored to use `ComponentRuntime` internally (no
  behavior change for apps).
- Action dispatch reuses `focus.cpp` (`handleFocusKey` generalized to Action).
- Pointer dispatch reuses `hit_test.cpp` from Plan 29.

## Bagian B — IScreen jadi component-based

`IScreen` mendapat jalur `build()`; `ViewDispatcher` merender screen via
`ComponentRuntime` + meneruskan Action **dan** Pointer ke sana.

```cpp
struct IScreen {
    virtual ~IScreen() = default;
    virtual void enter() {}

    // NEW primary path: return a component tree for this frame.
    // Default returns nullptr → screen still uses legacy draw()/onAction().
    virtual ui::UiNode* build(ui::NodeArena& arena, Runtime& rt) { return nullptr; }

    // Legacy (kept during migration; removed once all screens ported):
    virtual void onAction(input::Action a) { update(input::keyFromAction(a)); }
    virtual void onCode(input::Code) {}
    virtual void update(Key) {}
    virtual void draw(Canvas&) {}

    virtual ScreenMode mode() const { return ScreenMode::Normal; }
    // modal sizes unchanged …
};
```

`GuiService`/`ViewDispatcher` per frame:
```cpp
if (auto* root = active->build(arena, rt_)) {
    // component path: status bar drawn by GuiService, content via runtime
    runtime_.renderFrame(root, canvas, 0, contentY, w, contentH);
    // input: route action + pointer through runtime_ (Cancel/Back still pops)
} else {
    // legacy path: active->draw(canvas) / active->onAction(a)
}
```

This lets us migrate screens **one at a time**: a ported screen returns a tree;
an unported one keeps working via the legacy path. Once all 8 are done, delete
`update()`/`draw()` from `IScreen`.

## Bagian C — Migrasi 8 screens

Tiap screen: hapus `cursor_`/`scroll_` + imperative draw; ganti dengan `build()`
yang return tree pakai builder yang ada (`Header`, `Menu`, `Button`, `Text`,
`Container`, dan `ScrollView` dari Plan 31 untuk list panjang).

| Screen | Bentuk component |
|---|---|
| **HomeScreen** | `Container(Header("KAIRO"), Menu([Apps, Logs, Settings]))` — Menu item = Pressable → push screen |
| **SettingsScreen** | `Container(Header("SETTINGS"), Menu([WiFi?, Display, Controls, About]))` capability-gated |
| **AppListScreen** | `Container(Header("APPS"), ScrollView(Menu(plugins)))` → Pressable launch |
| **LogsScreen** | `Container(Header("LOGS"), ScrollView(Text per line))` |
| **AboutScreen** | `Container(Header("ABOUT"), Text rows + capability list)` |
| **SleepSettingsScreen** | `Container(Header("DISPLAY"), rows of [label + value stepper])` — stepper = AdjustDown/Up atau Pressable +/− (touch-friendly) |
| **LockScreen** | `Container(Text)` fullscreen; unlock via Activate×2 (unchanged logic) |
| **ControlsScreen** | `Container(Header("CONTROLS"), Text rows dari InputRegistry)` |

Catatan penting:
- **`cursor_` hilang** — focus system yang pegang seleksi (button mode). Touch =
  tap langsung. Highlight tampil hanya di button mode (Plan 29).
- **Footer hint** tetap pakai `rt.input().hintFor(Action)` (sudah dinamis).
- **Stepper di SleepSettings** sebaiknya jadi dua Pressable `<` `>` (atau `−`/`+`)
  agar bisa di-tap di touch, sekaligus tetap jalan via AdjustDown/AdjustUp di
  tombol. Ini contoh bagus dwi-modal.
- Screens yang butuh scroll (AppList, Logs) **menunggu ScrollView (Plan 31)** —
  bisa pakai versi non-scroll dulu (clip) lalu di-upgrade.

---

## File structure

```
firmware/core/
├─ include/kairo/ui/component_runtime.h   # ComponentRuntime
├─ src/ui/component_runtime.cpp
├─ include/kairo/ui/screen.h              ← + build(), legacy marked deprecated
├─ src/services/gui_service.cpp           ← component path vs legacy path
├─ src/app/component_app.cpp              ← use ComponentRuntime internally
└─ src/screens/*.cpp                      ← all 8 rewritten as build()
```

---

## Tasks

### Runtime
- [ ] `component_runtime.h/.cpp` — extract build/layout/focus/render + action/pointer dispatch
- [ ] Refactor `ComponentApp::run()` to use `ComponentRuntime` (no behavior change — verify apps still work)
- [ ] `IScreen::build()` added; `ViewDispatcher`/`GuiService` render component path when present, legacy path otherwise
- [ ] Route Action + PointerEvent from GuiService into the active screen's runtime; `Back`/Cancel still pops the stack

### Screen migration (one PR each, legacy path keeps others alive)
- [ ] HomeScreen → build()
- [ ] SettingsScreen → build() (capability-gated menu)
- [ ] AppListScreen → build() (+ ScrollView when Plan 31 lands)
- [ ] LogsScreen → build()
- [ ] AboutScreen → build()
- [ ] SleepSettingsScreen → build() (dual-modal stepper)
- [ ] LockScreen → build()
- [ ] ControlsScreen → build()

### Cleanup
- [ ] Remove `update(Key)` / `draw(Canvas&)` from `IScreen` once all migrated
- [ ] Remove now-unused imperative helpers (verify `components.cpp` drawTitle/modal still needed or fold into widgets)

## Acceptance criteria

- [ ] Apps behave identically after `ComponentRuntime` extraction (no regression)
- [ ] All 8 screens render via `build()`; no `cursor_`/`invertRect` left in screens
- [ ] On dev-board (buttons): every screen navigable with Up/Down/Select/Cancel exactly as before
- [ ] On SkyRizz (touch): every screen's menu items / buttons respond to direct tap
- [ ] Focus-ring shows only in button mode across all screens (`:focus-visible`)
- [ ] `grep -rn "cursor_\|invertRect" firmware/core/src/screens/` → 0 results
- [ ] `IScreen` no longer declares `update()`/`draw()` after cleanup

## Non-Goals

- ScrollView/scrollbar internals — Plan 31 (screens use clip/no-scroll interim)
- Gestures (swipe/momentum) — Plan 31
- Changing the ViewDispatcher navigation-stack model (push/pop stays)
- Turning screens into threads (they stay on the GUI thread, just component-based)
