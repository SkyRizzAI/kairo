# 27 — Input Abstraction & Board Keymap Registry

> Sistem input Palanu menjadi hardware-agnostic: screen/app diprogram terhadap
> **Intent (Action)**, bukan tombol fisik. Board baru cukup menyediakan satu
> keymap untuk langsung berfungsi — termasuk device 2-tombol gaya Ledger.

- Status: ☐ Not started
- Milestone: M8 (Hardware Portability)
- Depends on: 14 (UI Runtime / Key enum), 19.6 (GuiService), 24 (Config Store)
- Blocks: 28 (SkyRizz E32 Board Support)

---

## Latar belakang & motivasi

### Masalah sekarang

`Key` enum saat ini (`Up, Down, Left, Right, Select, Cancel`) mencampur dua hal:
- **Arah fisik** (Up vs Left) — bergantung hardware
- **Maksud navigasi** (prev/next/activate/back) — yang sebenarnya dibutuhkan screen

Semua screen diprogram langsung ke arah fisik. Akibatnya:
1. Board 3-tombol tanpa Up/Down → screen navigasinya patah
2. Footer hint hardcoded (`"Cancel back"`) → bohong di board non-6-tombol
3. Tidak ada cara formal untuk tahu gesture apa yang tersedia

### Referensi industri

| Sistem | Lapis fisik | Lapis intent | Gesture |
|---|---|---|---|
| **Web browser** | `KeyboardEvent.key` (`"ArrowUp"`, `"Enter"`) | — (app handle sendiri) | — |
| **Godot / Unity** | `Input.get_key(KEY_UP)` | `InputAction` ("ui_accept", "ui_cancel") | — |
| **Ledger cold wallet** | 2 tombol fisik | Next / Activate / Back | chord (kiri+kanan) = Activate |
| **Flipper Zero** | 5-button D-pad | — (hardcoded per screen) | — |
| **Palanu (Plan 27)** | `InputCode` (registry terbuka) | `InputAction` (floor guaranteed) | short/long/double/chord |

---

## Arsitektur: 2 lapis + gesture engine

```
Tombol fisik (GPIO / I2C expander)
   │
   ▼
[Gesture Engine per-board]   ← state machine: short/long/double/chord/repeat
   │  emit: PhysicalEvent { button_id, gesture }
   ▼
[IKeyMap per-board]          ← tabel: (button_id, gesture) → InputCode + InputAction
   │  emit: InputEvent { code, action }
   ▼
[InputService queue]         ← thread-safe, sudah ada, extended
   │
   ▼
[GuiService loop]
   │  → DPM intercept (existing)
   │  → IScreen::onAction(InputAction)   ← default app-facing API
   │  → IScreen::onCode(InputCode)       ← raw, untuk kasus khusus
   ▼
[IScreen]
```

### Lapis A — InputCode (registry terbuka)

Menggantikan / memperluas `Key` enum. Kode **geometris / fisik-ish**; bisa absen
di suatu board.

```cpp
namespace nema::input {

// Core codes — alias spasial dari intent
enum class Code : uint16_t {
    // Directional
    Up    = 0x01,
    Down  = 0x02,
    Left  = 0x03,
    Right = 0x04,

    // Semantic
    Enter  = 0x10,   // confirm / select
    Escape = 0x11,   // back / cancel
    Menu   = 0x12,   // context / hamburger

    // Extensible — app/board custom (namespace by convention)
    Custom = 0x8000, // custom codes >= 0x8000
};

// Untuk custom code per-board (contoh: touch.tap, camera, rotary)
// Board register via InputRegistry::registerCode(uint16_t id, const char* name)
// id harus >= Custom (0x8000)

} // namespace nema::input
```

Backward compat: `Key` enum lama di `key.h` dipetakan ke `InputCode` via
`Code codeFromKey(Key k)`. Tidak ada `#ifdef`, tidak ada breaking change.

### Lapis B — InputAction (floor guaranteed)

Intent navigasi — **tidak bergantung hardware**. Board WAJIB bisa menghasilkan
4 action inti (`Prev, Next, Activate, Back`) — divalidasi saat board init.

```cpp
namespace nema::input {

enum class Action : uint8_t {
    // === FLOOR WAJIB (4 ini harus selalu terjangkau) ===
    Prev     = 0x01,   // navigasi mundur (Up / Left / tombol-kiri)
    Next     = 0x02,   // navigasi maju  (Down / Right / tombol-kanan)
    Activate = 0x03,   // konfirmasi / enter
    Back     = 0x04,   // kembali / escape

    // === OPSIONAL (boleh absen) ===
    AdjustUp   = 0x11, // value++ (Right / long-right)
    AdjustDown = 0x12, // value-- (Left / long-left)
    Menu       = 0x13, // context menu

    None = 0x00,
};

// Theoretical minimum (non-goal v1, documented):
//   Next + Activate saja sudah cukup untuk device 1-tombol / 2-button minimal
//   Prev / Back boleh degradasi ke menu item UI jika benar-benar tidak tersedia

} // namespace nema::input
```

### Reduksi default: Code → Action

Setiap board mendapat reduksi default ini (bisa di-override di keymap):

| InputCode | Default Action |
|---|---|
| `Up` | `Prev` |
| `Down` | `Next` |
| `Left` | `AdjustDown` (fallback: `Prev`) |
| `Right` | `AdjustUp` (fallback: `Next`) |
| `Enter` | `Activate` |
| `Escape` | `Back` |
| `Menu` | `Menu` |

---

## Gesture Engine

State machine ringan yang hidup di dalam `IKeyMap` per-board. Mengubah edge
fisik mentah (press/release timestamp per button-line) menjadi gesture bersih.

```cpp
enum class Gesture : uint8_t {
    Short,      // press + release < long_threshold_ms
    Long,       // press + release >= long_threshold_ms
    Double,     // dua short-press dalam double_window_ms
    Chord,      // dua tombol press dalam chord_window_ms (Ledger-style)
    Repeat,     // hold-repeat setelah long_threshold_ms (scroll cepat)
};
```

### Parameter (tunable via Config Store)

| Parameter | Default | Config key |
|---|---|---|
| Long press threshold | 500 ms | `input/long_ms` |
| Double press window | 300 ms | `input/double_ms` |
| Chord window | 80 ms | `input/chord_ms` |
| Repeat rate | 150 ms | `input/repeat_ms` |

Parameter ini persisted via Plan 24 (Config Store) dan tampil di Input Info screen.
Ini satu-satunya "setting" yang aman diekspos — tidak bisa merusak floor guarantee.

### Catatan penting Chord

Chord butuh menahan emit single-press sampai window lewat (untuk membedakan
single-A vs chord-A+B). Akibatnya ada latensi kecil (`chord_window_ms`) pada
**semua tombol yang terlibat chord**. Aturan desain keymap: tombol yang paling
sering (Prev/Next) **hindari** chord agar nggak ada latensi felt.

---

## IKeyMap interface

Setiap board mengimplementasikan interface ini. Inilah "satu file per board" yang
memegang semua kerumitan gesture + mapping.

```cpp
namespace nema::input {

struct PhysicalEvent {
    uint8_t  button_id;  // board-defined button index
    Gesture  gesture;
    uint64_t timestamp_ms;
};

struct InputEvent {
    Code     code;
    Action   action;
    Gesture  gesture;    // info mentah, jarang dipakai app
};

class IKeyMap {
public:
    virtual ~IKeyMap() = default;

    // Dipanggil dari thread polling tombol
    // Gesture engine internal; emit ke InputService via callback
    virtual void feedEdge(uint8_t button_id, bool pressed, uint64_t now_ms) = 0;
    virtual void tick(uint64_t now_ms) = 0;  // untuk timeout long/double/chord

    // Introspection (read-only)
    virtual const char* boardName()   const = 0;  // "skyrizz-e32", "dev-board", dll.
    virtual int         buttonCount() const = 0;
    virtual const char* buttonLabel(uint8_t id) const = 0;  // "Left", "Middle", "Right"
    virtual Gesture     buttonGestures(uint8_t id) const = 0; // bitmask gesture supported

    // Hint API — board-specific label untuk suatu action
    // Contoh: Action::Back → "Hold ●" (3-btn) atau "Cancel" (6-btn) atau "◀+▶" (2-btn)
    virtual const char* hintFor(Action a) const = 0;

    // Floor validation — dipanggil saat board init
    // Return false → fail loud (log fatal + halt)
    bool validateFloor() const;  // implemented di base, check Activate+Back+Prev+Next reachable

protected:
    std::function<void(InputEvent)> emit_;  // set by InputService
    friend class InputService;
};

} // namespace nema::input
```

### Contoh keymap 6-tombol (dev board 0)

```cpp
// Sederhana — tidak ada chord, tidak ada long, semua short
// button 0=Left 1=Down 2=Up 3=Right 4=Select 5=Cancel
const char* DevBoardKeyMap::hintFor(Action a) const {
    switch (a) {
        case Action::Prev:     return "Up";
        case Action::Next:     return "Down";
        case Action::Activate: return "Select";
        case Action::Back:     return "Cancel";
        case Action::AdjustUp: return "Right";
        case Action::AdjustDown: return "Left";
        default: return "";
    }
}
```

### Contoh keymap 3-tombol (SkyRizz E32)

```cpp
// button 0=Kiri 1=Tengah 2=Kanan
// Short: Kiri=Prev, Tengah=Activate, Kanan=Next
// Long:  Tengah.long=Back
// AdjustUp/Down → fallback ke Next/Prev (nggak punya dedicated)
const char* E32KeyMap::hintFor(Action a) const {
    switch (a) {
        case Action::Prev:     return "◀";
        case Action::Next:     return "▶";
        case Action::Activate: return "●";
        case Action::Back:     return "Hold ●";
        default: return "";
    }
}
```

### Contoh keymap 2-tombol (Ledger-style, untuk referensi)

```cpp
// button 0=Kiri 1=Kanan
// Short: Kiri=Prev, Kanan=Next
// Chord: Kiri+Kanan = Activate
// Back: degradasi ke item UI "← Back" (nggak ada gesture khusus)
const char* LedgerKeyMap::hintFor(Action a) const {
    switch (a) {
        case Action::Prev:     return "◀";
        case Action::Next:     return "▶";
        case Action::Activate: return "◀+▶";
        case Action::Back:     return "Back (menu)";  // UI element
        default: return "";
    }
}
```

---

## Capability Query

`CapabilityRegistry` (sudah ada) diperluas untuk deklarasi InputCode yang tersedia
di board ini. Screen/app query ini untuk pilih mode render yang tepat.

```cpp
// Board deklarasikan saat describeHardware():
rt.capabilities().add("input.prev");
rt.capabilities().add("input.next");
rt.capabilities().add("input.activate");
rt.capabilities().add("input.back");
// opsional:
rt.capabilities().add("input.adjust");   // ada AdjustUp + AdjustDown
rt.capabilities().add("input.2d");       // ada Up+Down+Left+Right (full 2D nav)
rt.capabilities().add("input.touch");    // ada touch driver

// Screen query:
if (rt.capabilities().has("input.2d")) {
    // render keyboard grid 2D
} else {
    // render keyboard cursor 1D
}
```

---

## IScreen migration — backward compat

`IScreen` mendapat dua method baru; method lama `update(Key)` tetap ada.

```cpp
struct IScreen {
    // EXISTING — tetap ada, backward compat
    virtual void update(Key key) {}

    // NEW — default implementasi forward ke update() via actionToKey()
    virtual void onAction(InputAction a)  { update(actionToKey(a.action)); }
    virtual void onCode  (InputCode c)    {}   // raw, jarang dipakai

    // ... draw, tick, enter, mode — unchanged
};
```

`GuiService` dispatch ke `onAction()`. Screen lama otomatis tetap jalan via
default forward. Screen baru override `onAction()` langsung — lebih bersih.

Hint footer di screen bisa pakai API hint:

```cpp
// Sebelum (hardcoded, bohong di board lain):
c.drawText(4, ui::footerY(c.height()), "< > change  Cancel back", true);

// Sesudah (dinamis, benar di semua board):
char hint[64];
snprintf(hint, sizeof(hint), "%s/%s change  %s back",
    rt_.input().hintFor(Action::AdjustDown),
    rt_.input().hintFor(Action::AdjustUp),
    rt_.input().hintFor(Action::Back));
c.drawText(4, ui::footerY(c.height()), hint, true);
```

---

## InputRegistry — central registry

Singleton (dijangkau via `Runtime`) yang mengelola semua registrasi.

```cpp
class InputRegistry {
public:
    // Board registration
    void setKeyMap(std::unique_ptr<IKeyMap> km);
    IKeyMap& keyMap();

    // Introspection
    const char* hintFor(Action a) const;          // delegate ke keyMap
    bool        canReach(Action a) const;          // capability check
    bool        hasCode(Code c) const;             // specific code available?

    // Custom code registration (untuk board/app custom)
    void registerCode(uint16_t id, const char* name, const char* description);
    // id >= 0x8000; name = "touch.tap", "camera.trigger", dll.

    // Enumerate semua registered codes + actions (untuk introspection UI)
    void forEachCode  (std::function<void(Code, const char*)>) const;
    void forEachAction(std::function<void(Action, const char*, const char* hint)>) const;
};
```

---

## Controls / Input Info Screen (read-only introspection)

Layar informatif yang tampilkan apa yang terdaftar. Masuk di Settings → Controls.

```
┌─────────────────────────────┐
│ Controls                    │
├─────────────────────────────┤
│ Board: skyrizz-e32          │
│ Buttons: 3                  │
├─────────────────────────────┤
│ ACTIONS                     │
│  Prev      ◀                │
│  Next      ▶                │
│  Activate  ●                │
│  Back      Hold ●           │
├─────────────────────────────┤
│ GESTURES                    │
│  Short press  < 500ms       │
│  Long press   ≥ 500ms       │
├─────────────────────────────┤
│ REGISTERED CODES            │
│  Up Down Enter Escape       │
│  Left Right                 │
│  touch.tap [custom]         │
└─────────────────────────────┘
   ▶ next  Hold ● back
```

- Data sepenuhnya dari `InputRegistry` — nol hardcoded.
- Footer hint pakai `hintFor()` → otomatis benar per board.
- Read-only — tidak ada editing.

---

## File structure

```
firmware/core/
├─ include/palanu/input/
│  ├─ input_code.h        # InputCode enum + codeFromKey() compat
│  ├─ input_action.h      # InputAction enum
│  ├─ input_event.h       # InputEvent struct (extended: code+action+gesture)  ← extend existing
│  ├─ gesture.h           # Gesture enum + GestureEngine class
│  ├─ i_key_map.h         # IKeyMap interface + validateFloor()
│  └─ input_registry.h    # InputRegistry singleton
├─ src/input/
│  ├─ input_code.cpp      # codeFromKey(), code names
│  ├─ input_action.cpp    # action names, actionToKey() compat
│  ├─ gesture.cpp         # GestureEngine state machine
│  ├─ i_key_map.cpp       # validateFloor() implementation
│  └─ input_registry.cpp  # InputRegistry
├─ include/palanu/services/
│  └─ input_service.h     ← minor extend: setKeyMap(), hintFor() passthrough
├─ src/services/
│  └─ input_service.cpp
├─ include/palanu/ui/
│  └─ screen.h            ← tambah onAction() + onCode() dengan default forward
├─ src/screens/
│  └─ controls_screen.cpp # Input Info screen (read-only)
└─ src/services/
   └─ gui_service.cpp     ← dispatch ke onAction() instead of update(Key)
```

Existing:
- `firmware/boards/dev-board/src/tca9534_buttons.cpp` → wrap dengan keymap baru
- `firmware/core/include/palanu/ui/key.h` → tetap ada, `Key` alias ke `InputCode`

---

## Tasks

- [ ] `input_code.h/cpp` — `InputCode` enum + `codeFromKey()` compat + registry open extension
- [ ] `input_action.h/cpp` — `InputAction` enum + `actionToKey()` + action names
- [ ] `gesture.h/cpp` — `Gesture` enum + `GestureEngine` state machine (short/long/double/chord/repeat)
- [ ] `i_key_map.h/cpp` — `IKeyMap` interface + `validateFloor()` + hint API
- [ ] `input_registry.h/cpp` — `InputRegistry` (setKeyMap, hintFor, canReach, enumerate)
- [ ] `input_service.h/cpp` — extend: accept `IKeyMap`, expose `hintFor()`
- [ ] `screen.h` — tambah `onAction()` + `onCode()` dengan default forward ke `update(Key)`
- [ ] `gui_service.cpp` — dispatch ke `onAction()` instead of `handleKey(Key)` langsung
- [ ] DevBoard keymap — wrap `TCA9534Buttons` ke `IKeyMap` baru (6-tombol, semua short)
- [ ] `controls_screen.cpp` — Input Info screen read-only
- [ ] Tambah "Controls" ke Settings navigation
- [ ] `input/long_ms`, `input/chord_ms` dll. dibaca dari Config Store dengan default
- [ ] Verifikasi floor guarantee dev board 0 tetap pass
- [ ] Verifikasi screen lama jalan tanpa perubahan (via default forward `onAction→update`)

---

## Acceptance criteria

- [ ] Dev board 0: semua 6 tombol tetap berfungsi identik seperti sebelumnya (no regression)
- [ ] `inputRegistry.validateFloor()` → true untuk dev board (4 floor action terjangkau)
- [ ] `input.hintFor(Action::Back)` → `"Cancel"` di dev board, berbeda di board lain
- [ ] Footer hint `SleepSettingsScreen` dinamis (pakai hintFor), bukan hardcoded
- [ ] Controls screen tampil di Settings; data dari registry, bukan hardcoded
- [ ] Screen lama (`update(Key)`) tetap jalan via default `onAction()` forward — zero migration required
- [ ] Custom code `>= 0x8000` bisa didaftarkan dan muncul di Controls screen
- [ ] GestureEngine: short press < 500ms emit Short; tahan > 500ms emit Long (ditest via simulator inject timing)
- [ ] Chord: dua button dalam 80ms window emit Chord, bukan dua Short terpisah

---

## Non-Goals (v1)

- **Remap tombol oleh user** — didokumentasikan sebagai future; butuh validator floor + reset-to-default + safe-mode boot
- **Rotary encoder support** — masuk sebagai custom code nanti, bukan sekarang
- **Touch gesture complex** (swipe, pinch) — plan terpisah
- **Per-app keymap override** — future
- **Fractional gesture timing** — integer ms cukup untuk embedded
