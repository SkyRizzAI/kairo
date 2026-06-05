# 33 — Board Profile & Hardware Visualization

> Setiap board mendefinisikan **BoardProfile** — structured data yang mendeskripsikan
> layout fisik komponen (display, tombol, LED, sensor, dll.) dalam koordinat
> ternormalisasi. Profile ini di-render secara dinamis menjadi **ASCII art** di
> device (About screen) dan nantinya menjadi **SVG/HTML** di Kairo Forge (web).
> Satu sumber data, banyak renderer — posisi dijamin konsisten.

- Status: ☐ Not started
- Milestone: M9 (Ecosystem & Tooling)
- Depends on: 08 (System Introspection), 14 (UI Runtime), 27 (Input Abstraction)
- Blocks: — (Forge will consume this later)

---

## Latar belakang & motivasi

### Masalah sekarang

`HardwareRegistry` dan `CapabilityRegistry` hanya menyimpan metadata flat:
```cpp
struct HardwareEntry {
    std::string id;       // "buttons"
    DriverKind  kind;     // Other
    std::string detail;   // "TCA9534 6-button"
};
```

Tidak ada informasi **spasial** — di mana tombol secara fisik, seberapa besar display,
bagaimana komponen tersusun di board. Button layout hanya ada di **komentar kode**
(`board_config.h:61-69`):

```cpp
// 3 buttons BELOW the LCD: Left | OK/Back | Right
// 2 buttons on the RIGHT SIDE of the LCD: Up (top) | Down (bottom)
```

Ini tidak bisa di-query, tidak bisa di-render, dan tidak bisa dikonsumsi tooling
eksternal.

### Mengapa ini penting

Kairo adalah **open-source firmware** yang berjalan di banyak board berbeda —
bukan satu device fixed seperti Flipper Zero. Setiap board punya layout fisik unik.
Ekosistem Kairo butuh:

1. **Device** — About screen menampilkan visualisasi board sendiri (ASCII art +
   legend tombol dengan mapping keymap).
2. **Kairo Forge** (web app, future) — menerima BoardProfile via USB/BLE,
   render visualisasi interaktif, remote control dengan button positions yang
   akurat, flashing tools, dll.
3. **Komunitas** — board maker bisa mendefinisikan profile board mereka sendiri
   tanpa mengubah core firmware.

### Referensi industri

| Sistem | Board metadata | Layout fisik | Konsumsi eksternal |
|---|---|---|---|
| **Flipper Zero** | Hardcoded, satu board | Tidak ada | Desktop app hardcoded |
| **Arduino** | Board definition file (JSON) | Tidak ada | IDE board manager |
| **Zephyr RTOS** | Devicetree (.dts) | Tidak ada | Build system |
| **ESPHome** | YAML config | Tidak ada | Dashboard auto-generate |
| **Kairo (Plan 33)** | `BoardProfile` (C++ struct) | ✓ normalized x,y,w,h | JSON → Forge web |

Kairo unik karena membutuhkan **layout fisik** yang bisa di-render — bukan hanya
deklarasi komponen.

---

## Arsitektur: BoardProfile

### Data model

```cpp
// firmware/core/include/kairo/system/board_profile.h

namespace kairo {

enum class ComponentType : uint8_t {
    Display,
    Button,
    Led,
    Sensor,
    Speaker,
    Mic,
    Camera,
    Port,       // USB, expansion, etc.
    Other,
};

struct ComponentDef {
    uint8_t       id;         // nomor urut untuk visualisasi (1, 2, 3...)
    const char*   label;      // "Left", "OK/Back", "LCD", "RGB LED"
    ComponentType type;
    float         x, y;       // posisi top-left, normalized 0.0 – 1.0
    float         w, h;       // ukuran, normalized 0.0 – 1.0
};

struct BoardProfile {
    const char*          board_id;          // "skyrizz-e32"
    const char*          board_name;        // "SkyRizz E32"
    float                board_w, board_h;  // aspect ratio fisik (mm atau ratio)
    const ComponentDef*  components;
    uint8_t              component_count;
};

} // namespace kairo
```

### Koordinat ternormalisasi

Semua posisi dan ukuran dalam **range 0.0 – 1.0** relatif terhadap board outline.
Ini memastikan:

- **Renderer-agnostic** — ASCII grid 20×12, SVG 800×600, atau Canvas 400×300
  semua pakai data yang sama.
- **Resolution-independent** — tidak peduli berapa pixel/char yang tersedia.
- **Aspect ratio aware** — `board_w` dan `board_h` menyimpan proporsi fisik board
  (misal 80mm × 45mm → `board_w=80, board_h=45`), renderer bisa maintain aspect.

### Contoh: SkyRizz E32

```
Board fisik (approx 80mm × 55mm):
┌────────────────────────────┐
│                            │
│                            │ ④
│          LCD               │
│                            │ ⑤
│                            │
└────────────────────────────┘
 ①          ②          ③
```

```cpp
// firmware/boards/skyrizz-e32/include/kairo/skyrizze32/board_config.h

constexpr ComponentDef kE32Components[] = {
    // id  label      type               x      y      w      h
    { 1, "Left",    ComponentType::Button,  0.10f, 0.82f, 0.18f, 0.12f },
    { 2, "OK/Back", ComponentType::Button,  0.41f, 0.82f, 0.18f, 0.12f },
    { 3, "Right",   ComponentType::Button,  0.72f, 0.82f, 0.18f, 0.12f },
    { 4, "Up",      ComponentType::Button,  0.90f, 0.22f, 0.08f, 0.14f },
    { 5, "Down",    ComponentType::Button,  0.90f, 0.52f, 0.08f, 0.14f },
    { 6, "LCD",     ComponentType::Display, 0.04f, 0.04f, 0.82f, 0.72f },
};

constexpr BoardProfile kE32Profile = {
    "skyrizz-e32", "SkyRizz E32",
    80.0f, 55.0f,
    kE32Components, 6
};
```

### Contoh: Dev Board

```
Board fisik (approx 90mm × 55mm):
 ⑤ ┌────────────────────┐ ⑥
   │                    │
   │        LCD         │
   │                    │
   └────────────────────┘
 ①      ②      ③      ④
```

```cpp
// firmware/boards/dev-board/include/kairo/devboard/board_config.h

constexpr ComponentDef kDevComponents[] = {
    // id  label      type               x      y      w      h
    { 1, "Left",    ComponentType::Button,  0.05f, 0.82f, 0.15f, 0.12f },
    { 2, "Down",    ComponentType::Button,  0.27f, 0.82f, 0.15f, 0.12f },
    { 3, "Up",      ComponentType::Button,  0.49f, 0.82f, 0.15f, 0.12f },
    { 4, "Right",   ComponentType::Button,  0.71f, 0.82f, 0.15f, 0.12f },
    { 5, "OK",      ComponentType::Button,  0.02f, 0.15f, 0.08f, 0.14f },
    { 6, "Cancel",  ComponentType::Button,  0.90f, 0.15f, 0.08f, 0.14f },
    { 7, "LCD",     ComponentType::Display, 0.08f, 0.04f, 0.84f, 0.72f },
};

constexpr BoardProfile kDevProfile = {
    "dev-board", "Kairo Dev Board",
    90.0f, 55.0f,
    kDevComponents, 7
};
```

### Contoh: Simulator

```cpp
constexpr ComponentDef kSimComponents[] = {
    { 1, "Prev",     ComponentType::Button,  0.05f, 0.82f, 0.20f, 0.12f },
    { 2, "Next",     ComponentType::Button,  0.40f, 0.82f, 0.20f, 0.12f },
    { 3, "Activate", ComponentType::Button,  0.75f, 0.82f, 0.20f, 0.12f },
    { 4, "Display",  ComponentType::Display, 0.05f, 0.04f, 0.90f, 0.72f },
};

constexpr BoardProfile kSimProfile = {
    "simulator", "Kairo Simulator",
    16.0f, 9.0f,
    kSimComponents, 4
};
```

---

## IBoard integration

`IBoard` mendapat method baru untuk expose profile:

```cpp
// firmware/core/include/kairo/board.h

struct IBoard {
    virtual ~IBoard() = default;
    virtual const char* name() const = 0;
    virtual void describeHardware(Runtime& rt) = 0;

    // NEW — return board physical layout profile
    virtual const BoardProfile& profile() const = 0;
};
```

Setiap board implementasi return reference ke `constexpr` profile-nya:

```cpp
// SkyRizzE32
const BoardProfile& profile() const override { return kE32Profile; }

// DevBoard
const BoardProfile& profile() const override { return kDevProfile; }

// SimulatorBoard
const BoardProfile& profile() const override { return kSimProfile; }
```

---

## Phase 1 — BoardProfile Data Model

> **Goal:** BoardProfile struct terdefinisi, setiap board expose profile-nya
> via `IBoard::profile()`. Belum ada rendering.

### 1.1 Buat `board_profile.h`

File baru di `firmware/core/include/kairo/system/board_profile.h`.
Berisi `ComponentType`, `ComponentDef`, `BoardProfile` seperti di atas.

### 1.2 Extend `IBoard`

Tambah `virtual const BoardProfile& profile() const = 0;` di `board.h`.

### 1.3 Implementasi di setiap board

- `skyrizz_e32.h/cpp` — return `kE32Profile`
- `dev_board.h/cpp` — return `kDevProfile`
- `simulator_board.h/cpp` — return `kSimProfile`

### 1.4 Verifikasi Phase 1

```bash
# Build clean simulator + ESP32
cmake --build build

# Smoke test: rt.board().profile().board_id == "simulator" (sim)
# Smoke test: rt.board().profile().component_count > 0
```

---

## Phase 2 — AsciiRenderer

> **Goal:** BoardProfile di-render menjadi ASCII art secara dinamis dari data
> koordinat. Output: grid karakter yang bisa di-draw ke Canvas.

### 2.1 Desain AsciiRenderer

```cpp
// firmware/core/include/kairo/ui/ascii_board_renderer.h

namespace kairo::ui {

class AsciiBoardRenderer {
public:
    // Render board profile ke grid ASCII.
    // cols/rows = ukuran grid target (misal 24×14 untuk layar 264×176).
    // Output: vector of strings (satu string per baris).
    static std::vector<std::string> render(
        const BoardProfile& profile,
        uint8_t cols,
        uint8_t rows
    );

    // Render legend table (nomor → label → action).
    // action_fn: callback untuk lookup action dari button label.
    static std::vector<std::string> renderLegend(
        const BoardProfile& profile,
        std::function<const char*(const char* label)> action_fn
    );
};

} // namespace kairo::ui
```

### 2.2 Algoritma rendering

```
render(profile, cols, rows):
  1. Allocate grid[rows][cols] filled with ' '

  2. Hitung effective area dengan mempertimbangkan board aspect ratio:
     board_aspect = profile.board_w / profile.board_h
     grid_aspect  = cols / rows
     Jika board_aspect > grid_aspect → fit width, pad height
     Jika board_aspect < grid_aspect → fit height, pad width
     → ox, oy = offset (centering), ew, eh = effective size

  3. Untuk setiap component:
     gx = ox + round(component.x * ew)
     gy = oy + round(component.y * eh)
     gw = max(1, round(component.w * ew))
     gh = max(1, round(component.h * eh))

     Switch type:
       Display → draw box-drawing chars (┌─┐│└┘)
                  fill interior with ' '
                  center label text inside
       Button  → place circled number ①②③... at (gx, gy)
                  (atau "1","2","3" jika circled tidak tersedia di font)
       Led     → place "◉" + number
       Sensor  → place "◇" + number
       Speaker → place "♪" + number
       Mic     → place "🎤" + number (atau "M" + number)
       Camera  → place "◎" + number
       Port    → place "▭" + number
       Other   → place "?" + number

  4. Convert grid → vector<string>
```

### 2.3 Circled numbers

Untuk embedded display dengan font 5×8, circled numbers (①②③) mungkin tidak
tersedia. Fallback strategy:

| Font support | Render |
|---|---|
| Unicode circled tersedia | ① ② ③ ④ ⑤ ⑥ ⑦ ⑧ ⑨ |
| Hanya ASCII | [1] [2] [3] atau (1) (2) (3) |
| Pixel display 1-bit | Angka plain: 1, 2, 3 (tanpa bracket, hemat space) |

Deteksi: cek apakah `Canvas::drawText()` bisa render character > 0x7F.
Jika tidak, fallback ke plain numbers.

### 2.4 Contoh output

Grid 24×14, SkyRizz E32:

```
┌──────────────────────┐
│                      │
│                      │
│         LCD          │ 4
│                      │
│                      │ 5
│                      │
└──────────────────────┘
 1          2          3
```

Grid 24×14, Dev Board:

```
5 ┌────────────────────┐ 6
  │                    │
  │                    │
  │        LCD         │
  │                    │
  │                    │
  └────────────────────┘
 1      2      3      4
```

### 2.5 Legend rendering

```
renderLegend(profile, action_fn):
  Untuk setiap component dengan type == Button:
    label  = component.label
    action = action_fn(label)  // lookup dari IKeyMap
    output: "① Left    → Prev"

  action_fn implementation:
    // Cari di IKeyMap: button dengan label "Left" → hintFor action-nya
    // Atau iterate keymap entries untuk match label
```

Contoh output legend:

```
1 Left    → Prev
2 OK/Back → Activate/Back
3 Right   → Next
4 Up      → Adjust+
5 Down    → Adjust-
```

### 2.6 Verifikasi Phase 2

```bash
# Unit test: render kE32Profile di grid 24×14
# Verify: LCD box ada di area kiri-atas, button 1-3 di bawah, 4-5 di kanan
# Verify: tidak ada component yang keluar dari grid bounds
# Verify: legend output matches expected format

cmake --build build && ctest
```

---

## Phase 3 — About Screen Integration

> **Goal:** About screen menampilkan ASCII board visualization + legend dengan
> keymap actions. Data sepenuhnya dari BoardProfile + IKeyMap — zero hardcoded.

### 3.1 About screen layout

```
┌─────────────────────────────┐
│ ABOUT                       │
├─────────────────────────────┤
│ Board: skyrizz-e32          │
│ Plat:  esp32                │
│ FW:    0.8.0                │
│ Up:    2h 15m 30s           │
│                             │
│ ┌──────────────────────┐    │
│ │                      │    │
│ │         LCD          │ 4  │
│ │                      │    │
│ └──────────────────────┘ 5  │
│  1          2          3    │
│                             │
│ 1 Left    → Prev            │
│ 2 OK/Back → Activate/Back   │
│ 3 Right   → Next            │
│ 4 Up      → Adjust+         │
│ 5 Down    → Adjust-         │
│                             │
│ Capabilities:               │
│   display, wifi, input.2d   │
└─────────────────────────────┘
```

### 3.2 Implementasi

```cpp
// about_screen.cpp — extend build()

UiNode* AboutScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();

    // Existing: board, platform, firmware, uptime
    // ... (unchanged)

    // NEW: ASCII board visualization
    rows_.push_back("");
    auto ascii = ui::AsciiBoardRenderer::render(
        rt.board().profile(),
        cols_for_canvas(c.width()),   // e.g. c.width() / CHAR_W
        rows_for_area(c.height())     // portion of screen
    );
    for (auto& line : ascii) {
        rows_.push_back(line.c_str());
    }

    // NEW: Legend with keymap actions
    rows_.push_back("");
    auto legend = ui::AsciiBoardRenderer::renderLegend(
        rt.board().profile(),
        [&rt](const char* label) -> const char* {
            return rt.input().keyMap().actionForLabel(label);
        }
    );
    for (auto& line : legend) {
        rows_.push_back(line.c_str());
    }

    // Existing: capabilities
    // ... (unchanged)
}
```

### 3.3 IKeyMap extension

Tambah method untuk lookup action dari button label:

```cpp
// i_key_map.h — tambah:
virtual const char* actionForLabel(const char* label) const = 0;
```

Implementasi default iterate entries, match label, return action string.

### 3.4 Verifikasi Phase 3

```bash
# Build + run simulator
cmake --build build && ./build/kairo-sim

# Navigate: Settings → About
# Verify: ASCII art muncul, posisi komponen benar
# Verify: Legend menampilkan action dari keymap
# Verify: Scroll berfungsi jika konten panjang

# ESP32: flash + navigate ke About
bun run flash:esp32
# Verify: rendering identik (minus resolution difference)
```

---

## Phase 4 — JSON Serialization (untuk Forge)

> **Goal:** BoardProfile bisa di-serialize ke JSON untuk dikonsumsi Kairo Forge
> via USB/BLE bridge.

### 4.1 JSON schema

```json
{
  "board_id": "skyrizz-e32",
  "board_name": "SkyRizz E32",
  "board_w": 80.0,
  "board_h": 55.0,
  "components": [
    {
      "id": 1,
      "label": "Left",
      "type": "button",
      "x": 0.10,
      "y": 0.82,
      "w": 0.18,
      "h": 0.12
    },
    {
      "id": 6,
      "label": "LCD",
      "type": "display",
      "x": 0.04,
      "y": 0.04,
      "w": 0.82,
      "h": 0.72
    }
  ]
}
```

### 4.2 Serializer

```cpp
// firmware/core/include/kairo/system/board_profile_json.h

namespace kairo {

// Serialize BoardProfile ke JSON string (nlohmann::json)
std::string boardProfileToJson(const BoardProfile& profile);

// Serialize BoardProfile + live data (capabilities, keymap, system info)
// untuk Forge "board info" endpoint
std::string boardInfoToJson(Runtime& rt);

} // namespace kairo
```

### 4.3 Stdio bridge command

Extend stdio bridge protocol (Plan 09) dengan command baru:

```json
// Request:
{"cmd": "board.profile"}

// Response:
{"type": "board.profile", "data": { /* BoardProfile JSON */ }}
```

### 4.4 Verifikasi Phase 4

```bash
# Send command via stdio bridge
echo '{"cmd":"board.profile"}' | ./build/kairo-sim

# Verify: JSON output matches schema
# Verify: all components present with correct coordinates
# Verify: parseable by TypeScript (Forge side)
```

---

## File structure

### New files

```
firmware/core/
├─ include/kairo/system/
│  ├─ board_profile.h           # ComponentType, ComponentDef, BoardProfile
│  └─ board_profile_json.h      # boardProfileToJson(), boardInfoToJson()
├─ include/kairo/ui/
│  └─ ascii_board_renderer.h    # AsciiBoardRenderer
├─ src/system/
│  └─ board_profile_json.cpp    # JSON serialization
└─ src/ui/
   └─ ascii_board_renderer.cpp  # render() + renderLegend()
```

### Modified files

```
firmware/core/
├─ include/kairo/board.h        # tambah profile() pure virtual
├─ include/kairo/input/i_key_map.h  # tambah actionForLabel()
├─ src/screens/about_screen.cpp # integrate AsciiBoardRenderer
├─ CMakeLists.txt               # tambah new sources

firmware/boards/
├─ skyrizz-e32/
│  ├─ include/.../board_config.h  # tambah kE32Components[] + kE32Profile
│  └─ src/skyrizz_e32.cpp         # implement profile()
├─ dev-board/
│  ├─ include/.../board_config.h  # tambah kDevComponents[] + kDevProfile
│  └─ src/dev_board.cpp           # implement profile()
└─ simulator/
   ├─ include/.../simulator_board.h  # tambah profile()
   └─ src/simulator_board.cpp        # implement profile() + kSimProfile
```

---

## Tasks

### Phase 1 — Data Model
- [ ] Buat `board_profile.h` — `ComponentType`, `ComponentDef`, `BoardProfile`
- [ ] Extend `IBoard` — tambah `virtual const BoardProfile& profile() const = 0`
- [ ] SkyRizz E32 — definisikan `kE32Components[]` + `kE32Profile` di `board_config.h`
- [ ] SkyRizz E32 — implementasi `profile()` di `skyrizz_e32.cpp`
- [ ] Dev Board — definisikan `kDevComponents[]` + `kDevProfile` di `board_config.h`
- [ ] Dev Board — implementasi `profile()` di `dev_board.cpp`
- [ ] Simulator — definisikan `kSimComponents[]` + `kSimProfile`
- [ ] Simulator — implementasi `profile()` di `simulator_board.cpp`
- [ ] Build clean simulator + ESP32

### Phase 2 — AsciiRenderer
- [ ] Buat `ascii_board_renderer.h/cpp`
- [ ] Implementasi `render()` — grid-based, aspect-ratio-aware, box-drawing untuk display
- [ ] Implementasi `renderLegend()` — iterate buttons, lookup action
- [ ] Circled number fallback strategy (Unicode → plain ASCII)
- [ ] Unit test: verify positions match input coordinates
- [ ] Unit test: verify no overflow/out-of-bounds

### Phase 3 — About Screen
- [ ] Extend `IKeyMap` — tambah `actionForLabel()`
- [ ] Implementasi `actionForLabel()` di setiap board keymap
- [ ] Extend `about_screen.cpp` — integrate AsciiBoardRenderer
- [ ] Verify: About screen menampilkan ASCII art + legend
- [ ] Verify: scroll berfungsi jika konten panjang
- [ ] Verify: simulator + ESP32 rendering konsisten

### Phase 4 — JSON Serialization
- [ ] Buat `board_profile_json.h/cpp`
- [ ] Implementasi `boardProfileToJson()`
- [ ] Implementasi `boardInfoToJson()` (profile + capabilities + system info)
- [ ] Extend stdio bridge — tambah `board.profile` command
- [ ] Verify: JSON output parseable + schema-correct

---

## Acceptance criteria

### Phase 1
- [ ] `rt.board().profile()` return valid `BoardProfile` di semua board (sim, dev-board, skyrizz-e32)
- [ ] `profile().component_count > 0` untuk semua board
- [ ] Build clean simulator + ESP32

### Phase 2
- [ ] `AsciiBoardRenderer::render(kE32Profile, 24, 14)` → ASCII art dengan LCD box di kiri-atas, button 1-3 di bawah, 4-5 di kanan
- [ ] `AsciiBoardRenderer::render(kDevProfile, 24, 14)` → ASCII art dengan LCD box di tengah, button 1-4 di bawah, 5-6 di samping
- [ ] Tidak ada component yang posisinya keluar dari grid bounds
- [ ] Legend output format: `"1 Label → Action"` per button

### Phase 3
- [ ] About screen menampilkan ASCII board visualization
- [ ] Legend menampilkan button label + action dari keymap
- [ ] Data 100% dari BoardProfile + IKeyMap — zero hardcoded strings
- [ ] Scroll berfungsi; About screen tetap usable meskipun profile besar

### Phase 4
- [ ] `boardProfileToJson()` output valid JSON sesuai schema
- [ ] Stdio bridge `board.profile` command return correct JSON
- [ ] JSON parseable oleh TypeScript (test: `JSON.parse()` di Bun)

---

## Non-Goals

- **Interactive Forge UI** — plan ini hanya data model + device-side rendering.
  Forge web app adalah project terpisah (`@kairo/forge` atau `@kairo/studio`).
- **Touch/tap pada visualisasi** — Forge bisa interactive nanti, bukan scope firmware.
- **3D rendering** — hanya 2D top-down view.
- **Board editor** — user tidak bisa edit layout dari device. Profile di-define
  di compile time oleh board maker.
- **Online board registry** — future: centralized catalog di mana board maker
  bisa submit profile. Plan ini hanya per-device profile.
- **PCB outline shape** — board outline diasumsikan rectangular. Non-rectangular
  boards (rounded corners, cutouts) di-approximate dengan bounding box.
- **Multi-display boards** — asumsi satu display utama. Board dengan multiple
  display bisa define multiple Display components tapi renderer hanya highlight satu.
