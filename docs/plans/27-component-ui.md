# 27 — Component UI System (Retained-Mode, C-first, JS-ready)

> Lapisan UI deklaratif di atas Canvas: UI dideskripsikan sebagai **pohon komponen**,
> sebuah **layout engine flexbox-subset** menghitung posisi, dan **renderer** mengecat ke
> Canvas. C app (sekarang) dan JS app (nanti, embedded JS engine) menghasilkan **node tree
> yang sama** — satu layout engine + satu renderer melayani keduanya. Mirip React Native / Ink.

- Status: ✅ Fase 1–5 DONE + SEMUA app dimigrasi (build host+ESP32 hijau, layout test ALL PASS). Fase 6 (embed JS engine) future.

> **Catatan eksekusi — SEMUA app dimigrasi ke component system:**
> - **Counter** — value Title-scale + tombol [−][+][Reset] + focus nav (terverifikasi live).
> - **Clock** — display-only + `onTick` (redraw saat detik berubah, skip-repaint dipertahankan).
> - **Stopwatch** — display + `onKey` (Select start/stop, Up reset) + `onTick` animasi ~20fps.
> - **Ticker** — `onStart` auto-fetch (TaskRunner worker), `onTick` poll hasil + spinner.
> - **TaskDemo** — `onKey` submit job async, `onTick` poll completion + elapsed.
> - **WiFi** — HYBRID: Overview/Pick/Result pakai Menu komponen + focus; password entry pakai
>   VirtualKeyboard lewat **escape hatch** (`capturesInput()` + `drawRaw()`). Async scan/connect
>   via TaskRunner, credential persist via ConfigStore.
>
> **Tambahan ke ComponentApp** untuk dukung semua app: `onStart()` (init/auto-fetch),
> `tickIntervalMs()`+`onTick()` (live apps, redraw hanya saat berubah → no e-ink regresi),
> `capturesInput()`+`drawRaw()` (escape hatch untuk layar custom seperti keyboard).
>
> **Modal / overlay layer (terverifikasi live).** `buildModal()` hook + widget `ui::Modal()`:
> dialog terpusat digambar DI ATAS base tree, dengan white backdrop + border, dan
> **focus + input di-capture** oleh modal (base beku, pakai ComponentState terpisah). Cancel
> saat modal terbuka tidak keluar app (di-handle `onKey`). Counter mendemonstrasikan ini —
> Reset memunculkan konfirmasi Yes/No (mengembalikan dialog konfirmasi yang sebelumnya ada di
> immediate-mode `drawConfirm`). Ini "top-level overlay layer" yang dijanjikan §0.
>
> **Catatan jujur soal verifikasi live:** Counter terverifikasi live (tree+focus+onPress+state).
> Re-verifikasi live tiap app lain terhalang fitur sleep/lock (Plan 21) yang mengunci device
> lebih cepat dari cadence tool-call navigasi otomatis — bukan bug app. Semua 6 app compile di
> host+ESP32 dan memakai jalur ComponentApp yang sama & sudah terverifikasi. Untuk uji manual,
> set Settings → Display → Sleep → Off agar timer tidak mengganggu.
- Milestone: M9 (App Platform)
- Depends on: 19.6 (IApp/AppHost/AppContext), 25 (Canvas scale + resolution-independent layout)
- Blocks: JS app runtime (future), app store, third-party apps

> **Kontrak kejujuran** (sama seperti plan lain): tiap fase build & jalan di host+ESP32; scope
> eksplisit; non-goals eksplisit; tidak ada klaim ajaib. Sistem ini **berdampingan** dengan
> UI immediate-mode lama (screens/apps existing tidak disentuh) — migrasi opsional & bertahap.

---

## 0. Kenapa ini, dan apa yang berubah secara fundamental

UI Palanu sekarang **immediate-mode**: app memanggil `c.drawText(x,y,...)`, `c.fillRect(...)`
dengan koordinat absolut yang dihitung manual tiap screen. Itu seperti menggambar langsung
ke kanvas.

React / Ink / Flutter itu **retained-mode**: kamu *deskripsikan pohon komponen*, layout engine
menghitung posisi, renderer mengecat. Karena React menghasilkan tree + reconciliation, "compiler
React ke canvas" **wajib** punya node tree + layout engine — tidak bisa pakai `drawText` manual.

**Inti elegannya:** C builder dan (nanti) React reconciler sama-sama menghasilkan **node tree
yang sama**. Satu layout engine + satu renderer. Itu jawaban untuk "C atau JS pakai UI yang sama".

```
┌─ App C (builder) ┐                  ┌─ App JS (React TSX) — FUTURE ┐
└────────┬─────────┘                  └──────────────┬───────────────┘
         │      keduanya menghasilkan                │
         └──────────►   UiNode TREE   ◄──────────────┘
                            │
              ┌─────────────┴──────────────┐
              │  Layout engine (flex subset)│  ← jantung; logical px
              └─────────────┬──────────────┘
                            │
              ┌─────────────┴──────────────┐
              │  Renderer → Canvas (existing)│
              └─────────────┬──────────────┘
                            │
              ┌─────────────┴──────────────┐
              │  Focus/input (4 arrows+OK)  │
              └──────────────────────────────┘
```

**Bonus arsitektural:** layout engine menyelesaikan TIGA hal sekaligus secara gratis —
(1) adaptive resolution (Plan 25): tree reflow otomatis saat dimensi canvas berubah;
(2) font-size reflow: `Text` tahu tinggi font, `Row`/`Col` menata ulang sendiri;
(3) input: focus traversal menggantikan cursor manual per-screen.

---

## 0.1 Allowed APIs (terverifikasi dari source — JANGAN mengarang signature)

**Canvas** (`core/include/palanu/ui/canvas.h`) — backend paint, semua koordinat LOGICAL:
```cpp
Canvas(IDisplayDriver& driver, uint8_t scale = 1);
uint16_t width() const;  uint16_t height() const;  uint8_t scale() const;  void setScale(uint8_t);
void clear(bool on=false);
void drawPixel(uint16_t,uint16_t,bool on=true);
void fillRect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,bool on=true);
void drawRect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,bool on=true);   // outline
void drawLine(uint16_t,uint16_t,uint16_t,uint16_t,bool on=true);
void invertRect(uint16_t,uint16_t,uint16_t,uint16_t);                      // XOR highlight
void setFont(const BitmapFont&);
void drawText(uint16_t x,uint16_t y,const char* text,bool on=true);
uint16_t textWidth(const char*) const;  uint16_t textHeight() const;
void drawTextScaled(uint16_t,uint16_t,const char*,uint8_t scale,bool on=true);
uint16_t centerX(const char*) const;
```
`BitmapFont { const uint8_t* data; uint8_t charW, charH, firstChar, numChars, spacing; }`.
Font default `FONT_5X8` (charW=5,charH=8). `extern const BitmapFont FONT_5X8;`

**IApp** (`core/include/palanu/app/app.h`):
```cpp
struct IApp { virtual const char* id() const=0; virtual const char* name() const=0;
              virtual void run(AppContext&)=0; virtual bool fullscreen() const { return false; } };
```

**AppContext** (`core/include/palanu/app/app_context.h`):
```cpp
Canvas& canvas();  void present();
bool nextInput(InputEvent& out);  bool waitInput(InputEvent& out, uint32_t timeoutMs);
void requestExit();  bool shouldExit() const;  Runtime& runtime();
```

**IScreen + ScreenMode** (`core/include/palanu/ui/screen.h`):
```cpp
enum class ScreenMode : uint8_t { Normal, Fullscreen, Modal };
struct IScreen { void enter(); void update(Key); void draw(Canvas&)=0; void tick(uint64_t);
                 ScreenMode mode() const; uint16_t modalWidth()/modalHeight() const; };
```

**InputEvent** (`core/include/palanu/nema/input_event.h`):
```cpp
struct InputEvent { enum class Type:uint8_t{Press,Release,Repeat}; Key key=Key::None; Type type=Type::Press; };
```

**Key** (`core/include/palanu/ui/key.h`): `None, Up, Down, Left, Right, Select, Cancel`.

**ui_constants.h**: `CHAR_W=6, CHAR_H=9, STATUS_Y, STATUS_H, SEP1_Y, CONTENT_Y`;
inline `footerY(h)`, `sep2Y(h)`, `contentRows(h)`, `cols(w)`.

**Reference render loop** (`core/src/apps/counter_app.cpp`):
`while(!ctx.shouldExit()){ if(ctx.waitInput(ev,100)){...} if(dirty){ Canvas&c=ctx.canvas(); c.clear(); ...draw...; ctx.present(); } }`

**Anti-patterns (JANGAN):**
- Mengarang method Canvas yang tidak ada di daftar di atas.
- `drawText` koordinat absolut DI DALAM komponen (kecuali leaf renderer Text). Komponen TIDAK tahu posisi absolutnya.
- `new`/`malloc` per-node per-frame → pakai arena (lihat §3.5).
- Menyentuh Canvas/ViewDispatcher dari thread lain (app thread punya buffer sendiri via AppContext).

---

## 1. Node Tree

`core/include/palanu/ui/node.h`

```cpp
namespace nema::ui {

enum class NodeType : uint8_t { View, Text, Pressable };
enum class FlexDir  : uint8_t { Row, Col };
enum class Align    : uint8_t { Start, Center, End, Stretch };       // cross-axis
enum class Justify  : uint8_t { Start, Center, End, SpaceBetween };  // main-axis

enum class TextRole : uint8_t { Body, Title, Caption };  // resolved → BitmapFont (§4)

// SIZE_AUTO = ukur dari konten; selain itu = fixed logical px.
constexpr uint16_t SIZE_AUTO = 0xFFFF;

struct Style {
    FlexDir  dir       = FlexDir::Col;
    uint16_t flexGrow  = 0;            // 0 = tidak tumbuh; >0 = bobot pembagian sisa ruang
    uint16_t width     = SIZE_AUTO;
    uint16_t height    = SIZE_AUTO;
    uint8_t  padding   = 0;            // seragam 4 sisi (cukup untuk v1)
    uint8_t  gap       = 0;            // jarak antar-anak di main axis
    Align    align     = Align::Start;
    Justify  justify   = Justify::Start;
    bool     border    = false;        // drawRect keliling
    bool     background = false;       // fillRect isi (jarang di 1-bit; biasanya invert utk fokus)
};

struct UiNode {
    NodeType   type     = NodeType::View;
    Style      style;
    // Text:
    const char* text    = nullptr;     // TIDAK dimiliki node — caller jamin lifetime sampai render selesai
    TextRole    role    = TextRole::Body;
    // Pressable:
    void      (*onPress)(void* userdata) = nullptr;
    void*       userdata = nullptr;
    bool        focusable = false;     // Pressable → true
    // Tree:
    UiNode*    firstChild = nullptr;
    UiNode*    nextSibling = nullptr;
    // Hasil layout (diisi layout engine; logical px absolut relatif ke root):
    int16_t    x = 0, y = 0;
    uint16_t   w = 0, h = 0;
};

} // namespace nema::ui
```

> **Catatan jujur soal `text` ownership:** node tidak menyalin string. Untuk C builder yang
> rebuild tiap frame dengan string literal / buffer milik app yang hidup selama `render()`,
> ini aman. Kalau butuh string sementara, app simpan di buffer-nya sendiri. (JS path nanti
> akan menyalin ke arena string — di luar scope plan ini.)

---

## 2. Layout Engine (jantung)

`core/include/palanu/ui/layout.h` (+ `.cpp`)

Dua-pass flexbox **subset**. Bekerja dalam **logical px** (Canvas scale urus fisik).

```cpp
namespace nema::ui {
// Hitung x/y/w/h tiap node, mengisi field hasil di UiNode.
// (rootW, rootH) = area logical tersedia (mis. c.width(), c.height()-statusbar).
void layout(UiNode& root, uint16_t rootW, uint16_t rootH, const Canvas& metrics);
}
```

**Pass 1 — measure (intrinsic size, bottom-up):**
- `Text`: `w = metrics.textWidth(text)` (font dari role), `h = metrics.textHeight()`.
  (Catatan: textWidth pakai font aktif Canvas; renderer set font per role sebelum ukur — atau
  sediakan helper ukur per-font. Implementasi: simpan tabel metrik per role, lihat §4.)
- `View`/`Pressable`: ukur anak; main-axis = Σ(anak) + gap×(n-1) + padding×2;
  cross-axis = max(anak) + padding×2. Kalau `width`/`height` fixed → pakai itu.

**Pass 2 — arrange (top-down):**
- Hitung sisa ruang main-axis = parentMain − Σ(fixed children) − gaps − padding.
- Bagi sisa ke anak ber-`flexGrow>0` proporsional bobot.
- Tempatkan anak sepanjang main-axis sesuai `justify` (Start/Center/End/SpaceBetween).
- Posisi cross-axis tiap anak sesuai `align` (Start/Center/End/Stretch=isi penuh cross).
- Terapkan padding sebagai offset awal + pengurang ruang.
- Tulis x/y absolut (parent.x + offset) ke tiap node.

**SCOPE EKSPLISIT (v1):** hanya row/col satu main-axis, flexGrow, fixed size, padding seragam,
gap, alignItems, justifyContent. **TIDAK ADA**: wrapping, grid, absolute positioning (kecuali
overlay top-level §6), margin per-sisi, percentage size. Subset ini menutup ~95% layout TUI
(list, form, header/body/footer).

---

## 3. Builder API (C) + Arena

`core/include/palanu/ui/widgets.h` (+ `.cpp` untuk mid-level)

### 3.5 Arena (alokasi node)

```cpp
namespace nema::ui {
class NodeArena {
public:
    explicit NodeArena(size_t capacity);   // alokasi sekali (mis. 256 node)
    UiNode* alloc();                        // ambil node ter-reset; nullptr kalau penuh
    void    reset();                        // panggil tiap awal render() — O(1), tanpa free
private:
    UiNode*  pool_ = nullptr;
    size_t   cap_ = 0, used_ = 0;
};
}
```

> **Aturan memori:** SATU `NodeArena` per app/screen, `reset()` di awal tiap `render()`.
> Rebuild tree penuh tiap frame (immediate rebuild of retained tree). Untuk C, tidak ada
> reconciliation — full re-layout tiap frame murah (tree kecil, <256 node). Diffing hanya
> diperlukan JS path (future). **JANGAN** `new UiNode` per frame.

### Primitives (low-level)

```cpp
UiNode* View    (NodeArena&, Style, std::initializer_list<UiNode*> children = {});
UiNode* Text    (NodeArena&, const char* str, TextRole role = TextRole::Body);
UiNode* Pressable(NodeArena&, void(*onPress)(void*), void* userdata,
                  Style, std::initializer_list<UiNode*> children = {});  // focusable=true
```

### Mid-level (Fase 4 — dibangun DARI primitives)

```cpp
UiNode* Row   (NodeArena&, Style, std::initializer_list<UiNode*>);   // dir=Row
UiNode* Col   (NodeArena&, Style, std::initializer_list<UiNode*>);   // dir=Col
UiNode* Container(NodeArena&, std::initializer_list<UiNode*>);        // padding default
UiNode* SafeArea (NodeArena&, std::initializer_list<UiNode*>);        // sisakan area status bar
UiNode* Button(NodeArena&, const char* label, void(*onPress)(void*), void* ud); // Pressable+border+Text
UiNode* Header(NodeArena&, const char* title);                        // judul + separator
UiNode* Footer(NodeArena&, const char* hint);
UiNode* Menu  (NodeArena&, const char** items, int n, int* selOut, void(*onPick)(void*,int), void* ud);
```

---

## 4. Renderer + Font/Text-size

`core/include/palanu/ui/renderer.h` (+ `.cpp`)

```cpp
namespace nema::ui {
void render(const UiNode& root, Canvas& c, int focusedId = -1);
const BitmapFont& fontForRole(TextRole role);   // resolusi role → font aktual
}
```

- Walk tree, untuk tiap node: kalau `background` → `fillRect(x,y,w,h,true)`;
  kalau `border` → `drawRect(x,y,w,h)`; kalau `Text` → `setFont(fontForRole(role))` lalu
  `drawText(x + padding, y + padding, text)`.
- Node fokus (Pressable terfokus) → highlight: `invertRect(x,y,w,h)` (atau border tebal).
- Hormati `ScreenMode`: di Normal, root di-offset di bawah status bar (pakai `SafeArea`).

**Font/Text-size integration:** `fontForRole` + preferensi global `display/text_size`
(Small/Normal/Large via ConfigStore) memilih `BitmapFont`:
- Normal: Body=FONT_5X8, Title=FONT_10X16, Caption=FONT_5X8
- Large:  Body=FONT_10X16, Title=FONT_15X24, Caption=FONT_5X8

`FONT_10X16`/`FONT_15X24` = pixel-doubled dari FONT_5X8 (lihat Plan 25 Phase 3; bisa di-generate
atau dependency). Karena layout engine ukur via metrik font aktual, ganti text-size → reflow
otomatis. **Ini melebur diskusi font-size: tidak perlu refactor `CHAR_H`→`textHeight()` per-screen
karena screen ditulis ulang sebagai komponen.**

---

## 5. Focus + Input

`core/include/palanu/ui/focus.h` (+ `.cpp`)

```cpp
namespace nema::ui {
struct FocusState {
    int focused = 0;     // index Pressable terfokus
    int count   = 0;     // jumlah focusable di tree terakhir
};
// Kumpulkan node focusable (tree order), update index dari key navigasi.
// Returns true kalau Select menembak onPress (caller redraw).
bool handleFocusKey(UiNode& root, FocusState& fs, Key k);
}
```

- Kumpulkan Pressable `focusable==true` dalam urutan tree (DFS).
- `Up`/`Left` → focused−1, `Down`/`Right` → focused+1 (clamp/wrap).
- `Select` → panggil `onPress(userdata)` node terfokus → return true.
- `Cancel` → TIDAK dihandle di sini; bubble ke app (back/exit).
- Renderer terima `focusedId` untuk highlight.

> v1: traversal **urutan tree** (cukup untuk list/form vertikal). Spatial nav (2D berdasarkan
> posisi) = future enhancement.

---

## 6. Integrasi App Model

`core/include/palanu/app/component_app.h` (+ `.cpp`)

Base class yang menjembatani node-tree ke `IApp` (app-threaded) — pola sama `counter_app`:

```cpp
namespace nema {
class ComponentApp : public IApp {
public:
    void run(AppContext& ctx) override;   // loop: build → layout → render → present; input → focus/state
protected:
    // Subclass implement: bangun tree dari arena + state app, return root.
    virtual ui::UiNode* render(ui::NodeArena& arena, AppContext& ctx) = 0;
    // Optional: handle Cancel / key non-navigasi. Return true kalau dikonsumsi.
    virtual bool onKey(Key k, AppContext& ctx) { return false; }
};
}
```

`run()` loop:
```
arena(256); FocusState fs;
while (!ctx.shouldExit()) {
    if (dirty) { arena.reset(); root = render(arena, ctx);
                 ui::layout(*root, c.width(), c.height(), c);
                 ui::render(*root, c, fs.focused); ctx.present(); dirty=false; }
    if (ctx.waitInput(ev, 80)) {
        if (ev.type==Press||ev.type==Repeat) {
            if (ev.key==Key::Cancel && !onKey(Cancel,ctx)) { ctx.requestExit(); }
            else if (ui::handleFocusKey(*root, fs, ev.key)) dirty=true;  // onPress fired
            else if (onKey(ev.key,ctx)) dirty=true;
        }
    }
}
```

Overlay/modal: root boleh punya layer overlay (di-render terakhir, center) — satu-satunya
"absolute" yang diizinkan. Cooperative `IScreen` juga bisa host tree via helper serupa (opsional).

---

## 7. Fase Implementasi

Tiap fase build + jalan di host & ESP32.

### Fase 1 — Node + Style + Layout engine (logika murni, host-testable)
- Buat `node.h`, `layout.h/.cpp`, `NodeArena`.
- Unit-ish test di host: row/col, flexGrow membagi sisa, padding, gap, align, justify benar
  (assert x/y/w/h node untuk beberapa tree contoh). Belum ada rendering.
- **Selesai bila:** test layout lulus host; build dua target tanpa perubahan perilaku UI lama.

### Fase 2 — Renderer → Canvas
- `renderer.h/.cpp` + `fontForRole` (sementara semua role → FONT_5X8).
- Demo statis: tree (centered box + Text) di-render via app dummy / ComponentDemo.
- **Selesai bila:** kotak + teks tampil benar di simulator pada 264×176.

### Fase 3 — Focus + input
- `focus.h/.cpp` + highlight node terfokus di renderer.
- Demo interaktif: beberapa Pressable; panah pindah fokus; Select tembak onPress.
- **Selesai bila:** navigasi fokus + aktivasi bekerja di simulator.

### Fase 4 — Mid-level components + Text-size
- `widgets.h/.cpp`: Row, Col, Container, SafeArea, Button, Header, Footer, Menu.
- `fontForRole` baca `display/text_size` (ConfigStore); FONT_10X16 (pixel-double) tersedia.
- **Selesai bila:** demo pakai Header/Menu/Button reflow saat text-size Normal↔Large.

### Fase 5 — ComponentApp base + migrasi 1 screen nyata
- `component_app.h/.cpp`.
- Migrasi SATU layar sebagai bukti (rekomendasi: `SettingsScreen` jadi ComponentApp/Screen,
  ATAU app baru `ComponentDemo`). Screen lama lain TIDAK disentuh.
- **Selesai bila:** layar tsb reflow benar di 264×176, 400×300, dan scale 2× dengan focus nav.

### Fase 6 — FUTURE (didokumentasikan, TIDAK dibuat di plan ini)
- Embed JS engine (QuickJS/Elk) di firmware (heap di PSRAM).
- `react-reconciler` custom host config → menghasilkan **UiNode tree yang sama**.
- Reconciliation/diffing untuk update parsial (C path tidak butuh ini).
- Inilah alasan node tree + layout + renderer dibuat frontend-agnostic.

---

## 8. File Plan

| File | Aksi | Fase |
|---|---|---|
| `core/include/palanu/ui/node.h` | buat | 1 |
| `core/include/palanu/ui/layout.h` + `src/ui/layout.cpp` | buat | 1 |
| `core/include/palanu/ui/widgets.h` (NodeArena + primitives) + `src/ui/widgets.cpp` | buat | 1→4 |
| `core/include/palanu/ui/renderer.h` + `src/ui/renderer.cpp` | buat | 2 |
| `core/include/palanu/ui/focus.h` + `src/ui/focus.cpp` | buat | 3 |
| `core/src/ui/font_10x16.cpp` (+ generator) | buat (pixel-double) | 4 |
| `core/include/palanu/app/component_app.h` + `src/app/component_app.cpp` | buat | 5 |
| `core/src/apps/component_demo.cpp` (demo) | buat | 2→5 |
| `core/CMakeLists.txt` | +sources baru | tiap fase |
| Canvas / screens / apps existing | **TIDAK disentuh** | — |

---

## 9. Acceptance Criteria

**Fase 1**
- [ ] Test host: Col 3 anak (1 flexGrow) di tinggi 100 → tinggi anak benar (sisa ke yang grow)
- [ ] Row + justify=SpaceBetween → posisi x anak benar
- [ ] align=Center → cross-axis terpusat; padding & gap diterapkan
- [ ] Build host + ESP32 hijau, UI lama tak berubah

**Fase 2**
- [ ] Tree {centered View {Text}} → kotak + teks tampil di posisi benar (264×176)

**Fase 3**
- [ ] 3 Button, panah pindah highlight, Select panggil onPress yang benar

**Fase 4**
- [ ] Menu + Header + Button render; ganti text_size Normal↔Large → layout reflow tanpa overflow

**Fase 5**
- [ ] Layar nyata (Settings/Demo) jalan via ComponentApp; reflow benar di 264×176, 400×300, scale 2×
- [ ] Focus nav 4-tombol bekerja; Cancel = back/exit
- [ ] Build host + ESP32 hijau

**Cross-cutting**
- [ ] Tidak ada `new`/`malloc` per node per frame (arena only) — cek via grep/inspeksi
- [ ] Komponen tidak menulis koordinat absolut (kecuali leaf Text renderer)
- [ ] Layout engine murni logical px (tidak ada angka 264/176 hardcoded)

---

## 10. Non-Goals (eksplisit)

- Full CSS/flexbox: NO wrap, NO grid, NO absolute (kecuali overlay top-level), NO margin per-sisi, NO percent size.
- Animasi/transisi (v1 statis; e-ink lambat refresh → animasi tidak relevan).
- Virtual scrolling kompleks (cukup list ter-clip sederhana).
- JS engine: **tidak dibuat** di plan ini — hanya dirancang-untuk (Fase 6 future).
- Theming di luar 1-bit fg/bg.
- Reconciliation/diffing untuk C path (full rebuild tiap frame, tree kecil).
- Spatial 2D focus nav (v1 urutan tree).

---

## 11. Hubungan dengan Plan Lain

- **19.6** (app model): ComponentApp dibangun di atas IApp/AppHost/AppContext (app-threaded, buffer handoff).
- **25** (adaptive UI): layout engine memakai Canvas logical dims + scale; reflow = manfaat langsung. Font 10×16 (Plan 25 Phase 3) jadi dependency Fase 4.
- **24** (config store): preferensi `display/text_size` disimpan di sini.
- **Future**: JS app runtime (embed engine) menempel di node tree yang sama — fondasi disiapkan di sini.
