# 52 — Aether UI SDK (Flipper-Style Pixelete + flexbox/node)

> UI SDK lengkap pertama: namespace `aether:ui`. Renderer pixelete 1-bit gaya
> Flipper, font/widget/theme milik Aether, layout flexbox + node-tree (Ink-style),
> compositor + surfaces. Di-expose ke 3 runtime.

- Status: ✅ Implemented — renderer/layout/widget/draw toolkit done (Plan 60); `SmartLabel(a, text)` widget added (TextRole::Smart); `setRenderTick(ms)` called by GuiService each frame; renderer `paint()` handles Smart text with `draw::marquee()` when focused and `draw::ellipsis()` otherwise, with `inFocused` flag propagated through the tree
- Depends on: 50 (UI SDK model), 51 (negosiasi), 53 (theming/assets)
- Blocks: 55 (surface compositing)

---

## Goals

- **Renderer pixelete** (crisp 1-bit, gaya Flipper gui) + rasterisasi font/text/
  shape/icon.
- **Pustaka widget Aether**: node-tree + layout flexbox (Ink-style) + modul ala
  Flipper (Submenu/Dialog/Popup/TextInput/List/Loading).
- **Raw canvas** sebagai escape hatch (game/kamera) di dalam SDK Aether.
- **Compositor**: memiliki N surface/viewport (status bar + app); kebijakan awal
  single-foreground (lihat Plan 55).
- Ekspos `aether:ui` ke C/WASM/JS via generator (Plan 49) + import (Plan 50).

## Keputusan

- Aether = UI SDK pertama & satu-satunya untuk sekarang; namespace `aether:ui`.
- **flexbox/node = milik Aether**, bukan core; server lain bebas pakai model lain.
- Pertahankan notion surface/viewport sejak awal = jahitan multi-window.

---

## Latar belakang

Aether **sudah ada sebagian besar** — yang dilakukan plan ini adalah
**memformalkan SDK + mengganti renderer**, bukan menulis dari nol. Audit kode
sekarang (semua di `firmware/core/`):

### Yang DIPERTAHANKAN (model node-tree + flexbox sudah matang)

| Lapisan | File | Kondisi sekarang | Nasib |
|---|---|---|---|
| **Model node** | `include/nema/ui/node.h:10,19,48` | `NodeType{View,Text,Pressable,Scroll,Slider}`, `Style` (FlexDir, flexGrow, w/h, padding, gap, Align, Justify, border, background), `UiNode` tree (`firstChild`/`nextSibling`), `ScrollState` caller-owned | **Dipertahankan utuh** — ini "Ink/React node" Aether |
| **Layout** | `include/nema/ui/layout.h:24`, `src/ui/layout.cpp` (225 LOC) | Flexbox-subset 2-pass: row/col, flexGrow, fixed size, padding, gap, alignItems, justifyContent. Pure logic, `TextMetrics` di-inject (host-testable) | **Dipertahankan** — tambah hanya yg perlu modul Flipper |
| **Widget builder** | `include/nema/ui/widgets.h`, `src/ui/widgets.cpp` (203 LOC) | `NodeArena` (pool fixed, reset O(1), **tanpa heap per-frame**) + 18 builder (View/Text/Pressable/ScrollView/Row/Col/Container/Button/Header/Footer/ListRow/Toggle/Stepper/Select/Slider/TextField/Menu/Modal) | **Dipertahankan + diperluas** (modul Flipper) |
| **Loop interaksi** | `include/nema/ui/component_runtime.h`, `.cpp` (239 LOC) | build→layout→focus→render + dispatch (nav Prev/Next/Activate, pointer tap-vs-drag, adjust ±1, flick momentum) | **Dipertahankan** |
| **Canvas 1-bit** | `include/nema/ui/canvas.h:8,26,70`, `src/ui/canvas.cpp` (~300 LOC) | Surface logical-pixel: primitives, clip, `BitmapFont` (`FONT_5X8`), `drawTextScaled`, **`blitRgb565` (escape hatch RGB565 sudah ada)** | **Dipertahankan** — jadi basis tier 0/1 |
| **Stack view** | `include/nema/ui/view_dispatcher.h` | Stack screen + redraw flag atomic | **Dipertahankan** |
| **Kontrak JS** | `src/js/js_engine.cpp:239` `reify()` | node-desc JS `{type, style, props, text, children}` → `UiNode` (bukti reconciler Ink-style jalan di QuickJS) | **Dipertahankan** — acuan binding 3 runtime |
| **Backend seam** | `include/nema/ui/display_server.h`, `aether_server.{h,cpp}` | `IDisplayServer` + `AetherServer::renderFrame` (komposit status bar + screen + modal + FPS) | **Diperluas jadi compositor** |

### Yang DITULIS-ULANG / BARU

- **Renderer** (`src/ui/renderer.cpp:7,85` — `paint()`/`render()`). Sekarang paint
  polos (fillRect/drawRect/drawText/invertRect). Di-upgrade ke **pixelete toolkit
  ber-tema**: frame gaya Flipper (sudut rounded, garis 1px crisp), separator,
  scrollbar bergaya, ikon, multiline + marquee. Renderer **membaca Theme**
  (Plan 53), bukan konstanta hardcoded. → **Ganti RENDERER, bukan buang model.**
- **Font/text/shape/icon rasterizer** — `text_style.{h,cpp}` (`FontSpec`,
  `fontForRole`, `setTextSize`) sekarang cuma role→font+scale. Diangkat jadi
  **tier-1 drawing toolkit** (`aether::ui::draw`): semua paint resmi lewat sini,
  baca Theme. Ikon/handle logis = baru (Plan 53); font v1 = `FONT_5X8` (monospace),
  tapi **format `Font` proporsional + multi-size sudah dikunci di Plan 53** (font
  asli menyusul, arsitektur sudah siap).
- **Compositor + Surface** — `AetherServer` sekarang nimpa 1 screen fullscreen ke
  Canvas. Diperluas jadi compositor N-surface (status bar + app), z-order,
  kebijakan v1 single-foreground (siap multi-window, Plan 55).
- **Modul widget ala Flipper** — Submenu/Dialog/Popup/TextInput/List/Loading
  sebagai retained "view module" (di atas node-tree + state persisten). Sebagian
  sudah ada benihnya (`text_input.cpp`, `virtual_keyboard.cpp`, `Menu`, `Modal`).

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr/LVGL) | Keputusan Aether (Palanu) |
|---|---|---|---|
| **Model UI** | Immediate-mode: tiap `View` punya `draw_callback(canvas, model)` — gambar manual tiap frame. Tak ada layout engine | LVGL retained `lv_obj` tree + flexbox bawaan, tapi **berat** (heap, anim engine) | **Node-tree retained + flexbox-subset** (`node.h`/`layout.cpp`) — deklaratif tanpa berat LVGL. Sudah ada |
| **Renderer** | `Canvas`→u8g2, 1-bit crisp. `elements.c` (33KB) = pustaka draw bersama: `frame`, `bubble`, `slightly_rounded_box`, `scrollbar`, `multiline`, `marquee` | LVGL draw (anti-alias, warna) compile-time | Tiru `elements.c`: **pixelete toolkit ber-tema** sbg tier-1, dipakai renderer node + raw |
| **Font handle** | `Font` enum: `FontPrimary/Secondary/Keyboard/BigNumbers/BatteryPercent` — handle logis, bukan bitmap mentah | `lv_font_t` statis di `fonts.c`, compile-time | Tiru: `TextRole`→handle logis, di-resolve **pack aktif** (Plan 53) → reskin global |
| **Ikon** | `Icon*` + `IconAnimation` (multi-frame, fps); `canvas_draw_icon` by name | Asset LVGL compile-time | `icon("status.wifi")` handle logis ber-namespace → pack resolve (Plan 53). Canvas sudah punya `drawBitmap` |
| **Modul GUI** | **View modules** retained: `submenu`, `dialog_ex` (text+3 tombol+ikon), `popup` (timed+ikon), `variable_item_list` (label+nilai cycling), `loading` (spinner), `text_input`/`byte_input`/`number_input`, `menu`, `text_box`, `empty_screen` | LVGL widgets (`lv_list`, `lv_btn`, …) | **Port idiom modul** jadi builder node Aether — bukan import-mode tiap frame, tapi sub-tree + state |
| **View stack/compositor** | `ViewDispatcher` + `Gui` layer (status bar + fullscreen + `view_stack` overlay). Status bar **persist** di atas app | LVGL screen tunggal, nimpa destruktif | `ViewDispatcher` ada; tambah **compositor surface** (status bar persist + app), v1 single-foreground |
| **Escape hatch** | `canvas_get_buffer()` direct framebuffer (game) | `lv_canvas` | `Canvas::blitRgb565` + raw `Canvas` surface (sudah ada) |
| **Swap renderer** | Hardwired u8g2, tak bisa diganti | **Compile-time** Kconfig | Aether = satu `IDisplayServer` swappable runtime (Plan 43) — lebih maju |

> Inti: Flipper kuat di **modul GUI** + **font/icon handle** + **elements toolkit**,
> tapi immediate-mode (tak ada layout). AkiraOS punya layout (LVGL) tapi
> compile-time & berat. Aether = **gabungan terbaik**: node+flexbox ringan
> (sudah ada) + pixelete renderer & modul gaya Flipper + handle logis reskin-able.

---

## Desain teknis

### Tiga tier akses UI (di dalam SDK Aether)

```
┌─ Tier 2: node-tree deklaratif (DEFAULT) ──────────────────────────┐
│  widgets.h builder → UiNode tree → layout() → renderComponentFrame │
│  App "mendeskripsikan" UI; runtime layout+paint+input otomatis.    │
├─ Tier 1: drawing toolkit ber-tema (aether::ui::draw) ─────────────┤
│  text(role)/icon(handle)/frame/box/separator/scrollbar — BACA Theme│
│  Untuk layar custom yg tak cocok node-tree, tapi tetap on-theme.   │
├─ Tier 0: raw canvas (escape hatch) ───────────────────────────────┤
│  Canvas primitives + blitRgb565 — game/kamera, BYPASS tema+layout. │
└────────────────────────────────────────────────────────────────────┘
   Semua tier menulis ke Surface yang sama (lihat Compositor).
```

```cpp
namespace aether::ui {

// Tier 0 — raw canvas (sudah ada: nema::Canvas).
nema::Canvas& Surface::canvas();          // primitives + blitRgb565

// Tier 1 — themed drawing toolkit (mengangkat text_style.* + elements ala Flipper).
namespace draw {
    void text   (nema::Canvas&, int x, int y, const char* s, TextRole role);
    void icon   (nema::Canvas&, int x, int y, IconHandle h);   // handle logis (Plan 53)
    void frame  (nema::Canvas&, Rect r);                       // border 1px, sudut rounded
    void box    (nema::Canvas&, Rect r, bool filled);          // slightly_rounded_box
    void separator(nema::Canvas&, int y);
    // scrollbar DASHED: thumb solid di atas TRACK garis putus-putus. Motif sama utk
    // vertikal (list) & horizontal (carousel MainMenu) — set `horizontal`.
    void scrollbar(nema::Canvas&, Rect track, int pos, int total, int view,
                   bool horizontal = false);
    void multiline(nema::Canvas&, Rect r, const char* s, TextRole role);
    void marquee (nema::Canvas&, Rect r, const char* s, TextRole role, uint32_t tick);
    void ellipsis(nema::Canvas&, Rect r, const char* s, TextRole role);  // potong + "…" bila kepanjangan
    // Chrome MainMenu (carousel DSi, lihat "Komponen krusial"):
    void banner  (nema::Canvas&, Rect r, const char* title, int notchX); // banner + notch ▼ ke tile
    void posbar  (nema::Canvas&, Rect track, int index, int count);      // indikator: segmen solid + dashed
}

// Tier 2 — node-tree (widgets.h, sudah ada). Default.
} // namespace aether::ui
```

### Pustaka widget Aether (daftar final)

**A. Primitif + komponen node (DIPERTAHANKAN dari `widgets.h`):**
`View`, `Text`, `Pressable`, `ScrollView` · `Row`, `Col`, `Container`, `Button`,
`Header`, `Footer`, `ListRow` · `Toggle`, `Stepper`, `Select`, `Slider`,
`TextField`, `Menu`, `Modal`.

**B. Modul ala Flipper (BARU — retained view module di atas node-tree + state):**

| Modul | Asal Flipper | Bentuk di Aether | State persisten |
|---|---|---|---|
| **Submenu** | `submenu.c` | Header + `ScrollView` berisi `ListRow`; auto-scroll fokus | `SubmenuState{selected, scroll}` |
| **Dialog** | `dialog_ex.c` | `Modal`: ikon + judul + teks + s/d 3 tombol (kiri/tengah/kanan via `Action`) | callback hasil |
| **Popup** | `popup.c` | Overlay terpusat: ikon + teks + auto-dismiss (timeout)/callback | `PopupState{deadlineMs}` |
| **TextInput** | `text_input.c` | Angkat `text_input.cpp` + `virtual_keyboard.cpp` jadi widget Aether resmi (mask password) | `TextInputState{buf,len}` |
| **List** (VariableItemList) | `variable_item_list.c` | `ScrollView` baris `Select`/`Stepper` (label + nilai cycling) + enter-callback | `ListState{selected, scroll}` |
| **Loading** | `loading.c` | Overlay spinner ber-animasi (anim handle, Plan 53); blok input | `LoadingState{tick}` |
| **(opsional nanti)** | `byte_input`/`number_input`/`text_box`/`empty_screen` | varian TextInput / ScrollView teks | — |

**C. Komponen krusial (BARU — anatomi konkret di "Komponen krusial" di bawah):**
`ListView` (baris ikon+label smart+aksesori, scrollbar dashed) · `SmartLabel`
(Text sadar fokus+lebar: biasa/ellipsis/marquee) · `FileBrowser` (varian ListView,
ikon per-tipe-file) · `MainMenu` (carousel gaya Nintendo DSi, module setingkat
Submenu). Plus tier-1 `draw::scrollbar` dashed (vertikal + horizontal).

> Semua modul B = **builder yang mengembalikan `UiNode*`** dari `NodeArena` +
> struct state caller-owned (pola `ScrollState` yang sudah ada). Bukan
> immediate-mode Flipper — tetap node-tree, jadi otomatis dapat layout, fokus,
> momentum, dan binding 3-runtime gratis.

### Komponen krusial (anatomi konkret)

Lima komponen ini = idiom yang **paling sering dipakai** OS UI. Semua dibangun dari
node-tree flexbox (`widgets.h`) + chrome tier-1 (`draw::*`) — bukan widget monolitik
baru. Ikon lewat icon system (Plan 53) ber-namespace.

**ListView** (komponen paling sering) — anatomi baris:

```
┌───────────────────────────────────────────┬─┐
│ [ikon kiri] [label smart .......... fill] [›]│┊│   ← scrollbar DASHED kanan
└───────────────────────────────────────────┴─┘
  fixed         flexGrow=1            fixed    track putus-putus
```

- Tiap baris = `Row` (`widgets.h:54`) flexbox: **ikon kiri** (`width` fixed, mis.
  `icon("file.folder")` 10×10) | **label** `flexGrow=1` (SmartLabel, di bawah) |
  **ikon aksesori kanan** fixed (chevron/centang, opsional, `width` fixed).
- Kontainer = `ScrollView` (`widgets.h:45`) align=Stretch; **scrollbar dashed**
  (`draw::scrollbar`, vertikal) di kanan viewport. Fokus & auto-scroll =
  `renderComponentFrame` (`component_runtime.h:47`).

**SmartLabel** — varian `Text` (`widgets.h:35`) yang **sadar fokus + lebar tersedia**:

| Kondisi | Perilaku |
|---|---|
| Teks **muat** di `w` baris | render biasa (`draw::text`) |
| Kepanjangan **& tak fokus** | truncate `…` (`draw::ellipsis`) |
| Kepanjangan **& fokus** | **marquee** geser (`draw::marquee`, tick-driven) |

Lebar tersedia = hasil `layout()` (`w` node, `layout.h:23`); status fokus + `tick`
dari `ComponentState`/`tickMomentum` (`component_runtime.h:23,66`). `draw::marquee`
sudah ada di tier-1; tambah `draw::ellipsis` untuk jalur truncate.

**FileBrowser** — varian ListView: **ikon kiri = tipe** (folder vs file, resolve via
**ekstensi** ke icon system: `icon("file.folder")`, `icon("file.txt")`,
`icon("file.generic")` fallback — Plan 53 namespace `file.*`), label = SmartLabel
(nama file panjang → marquee saat fokus), aksesori kanan opsional (mis. ukuran/›),
scrollbar dashed. Sama tulang dengan ListView, beda **resolver ikon baris**.

**MainMenu (carousel gaya Nintendo DSi)** — module setingkat Submenu (retained view
module + state). Anatomi (dari `refs/images/mainmenu-dsi.png`):

```
        ╭───────────── Apps ─────────────╮          ← banner judul (rounded)
        ╰──────────────┬─╲╱─┬────────────╯          ← notch ▼ segitiga → tile terpilih
                   ╭────┴────╮
   ┌────┐  ┌────┐  ┃        ┃  ┌────┐  ┌────┐        ← deret tile-ikon horizontal (rounded)
   │ ☰  │  │ ✳  │  ┃  icon  ┃  │((•))│  │ 🜂 │
   └────┘  └────┘  ┃ START  ┃  └────┘  └────┘        ← tile terpilih: border TEBAL + sub-label
                   ╰─────────╯
   ▂▂▂▂▂▂┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄        ← indikator posisi: segmen solid + dashed
```

- Bangun dari **`Row`** node-tree (deret tile, ikon dari icon system Plan 53) +
  chrome tier-1: `draw::banner` (banner + notch ▼ ke tile terpilih), `draw::box`
  (tile rounded; terpilih = border tebal), `draw::posbar` (indikator segmen solid +
  garis putus-putus), `draw::scrollbar` horizontal motif sama. Tile terpilih di
  **tengah**, geser saat Prev/Next (carousel) — state `MainMenuState{selected}`.
- **Menu-style = opsi tema/settings** (ala Momentum, Plan 53): default = gaya DSi
  ini; alternatif (grid/list) bisa ditambah belakangan tanpa ubah registry.
- **Sumber item = registry app + ikon manifest** (Plan 59): launcher & MainMenu
  render ikon app lewat icon system yang sama (rujuk-silang Plan 53/59).

**scrollbar dashed** (tier-1 `draw::scrollbar`) — motif bersama: **thumb solid** di
atas **track garis putus-putus**. Orientasi **vertikal** (ListView/FileBrowser) &
**horizontal** (carousel MainMenu) — satu helper, flag `horizontal`.

### Responsive / scale / ratio (board-agnostic)

Melanjutkan resolution-independence Plan 25 — Aether menambah lapisan **scale**,
**reflow**, dan **fit mode** di atasnya, board-agnostic (tak ada cabang nama board).

**Base logical resolution** = lantai desain (mis. **128×64**, kelas Flipper). Semua
komponen menggambar dari `canvas.width()/height()` (Plan 25 §1) relatif terhadap
logical ini, bukan piksel fisik.

```
logical = physical / scale          // scale INTEGER (pixelete crisp, nearest-neighbor)
```

- **Scale auto** = integer **terbesar** yang menjaga `logical ≥ base`:
  - `256×128 → scale 2 → 128×64`
  - `320×240 → scale 2 → 160×120`
  - Manual override via `Theme.scale` / Settings (Plan 53 "Text Size").
- **Reflow (bukan cuma zoom)**: area logical mengisi flexbox → display lebih besar =
  **lebih banyak baris ListView kelihatan**, bukan baris yang membesar. Karena
  komponen sudah baca `canvas.width/height` (Plan 25), reflow **otomatis** — tak ada
  kode khusus per-board.
- **Fit mode**:

  | Mode | Arti | Crisp? |
  |---|---|---|
  | `fill` (default) | logical = physical/scale, **reflow** isi penuh | ✅ |
  | `letterbox` | jaga **rasio base** + center, scale integer, sisa = border | ✅ (base ratio) |
  | `stretch` | paksa penuh, rasio rusak | ❌ **tidak disarankan** (rusak crisp) |

- **Board deklarasi** physical res + dpi (`IDisplayDriver::dpi()`, Plan 25 §2.4) +
  **default fit mode**; renderer + `Theme.scale` (Plan 53) turunkan logical. Settings
  override. Ikon & font integer-scale crisp (nearest-neighbor — Plan 25, Plan 53
  icon system).

### Compositor + Surface

```cpp
namespace aether::ui {

struct Surface {                       // satu viewport drawable
    Rect        bounds;                // origin+size logical px (resolution-independent)
    uint8_t     z = 0;                 // z-order
    bool        visible = true;
    SurfaceKind kind;                  // NodeTree | Raw
    // NodeTree: punya NodeArena + root builder + ComponentState
    // Raw:      app menggambar langsung ke canvas() (game/kamera)
    nema::Canvas& canvas();
};

class Compositor {                     // menggantikan logika inline AetherServer
public:
    Surface* createSurface(Rect bounds, SurfaceKind kind);
    void     destroySurface(Surface*);
    void     setForeground(Surface*);  // kebijakan v1: single-foreground
    void     composite(nema::Canvas& out); // gambar surface menurut z-order + policy
private:
    // v1: status-bar surface (persist) + 1 app surface foreground.
    // siap multi-window: tinggal ganti WM policy (Plan 55), app TAK berubah.
};

} // namespace aether::ui
```

> **Catatan kepemilikan API surface:** `Compositor::createSurface(Rect, SurfaceKind)`
> di sini = alokasi **internal** compositor (geometri ditentukan WM policy). Entry
> **app-facing** = `aether::createSurface(ctx, SurfaceConfig)` yang mengembalikan
> `ISurface*` — didefinisikan di **Plan 55** (`ISurface`/`createSurface` + WM policy).
> App memanggil yang Plan 55; Aether internal memakai `Compositor::createSurface`.
> `SurfaceConfig` (Plan 55) **tanpa** width/height — compositor yang menentukan bounds.

`AetherServer::renderFrame` (`aether_server.cpp:11`) berubah: alih-alih nimpa 1
screen, ia **men-drive `Compositor::composite()`**. Status bar jadi surface
sendiri (persist), app jadi surface foreground. `RemoteScreenTap` (Plan 43) tetap
di layer `IDisplayDriver` → streaming ke Forge gratis lintas surface.

### Ekspos `aether:ui` ke 3 runtime (Plan 49/50)

Node-desc adalah kontrak — sudah terbukti di `js_engine.cpp:239` (`reify`):

```jsonc
// Bentuk node-desc (sumber kebenaran untuk generator Plan 49):
{ "type": "View|Text|Pressable|Scroll|Slider|Submenu|Dialog|...",
  "style": { "flexDirection":"row|column", "flexGrow":1, "padding":4, "gap":2,
             "width":120, "height":40, "alignItems":"center",
             "justifyContent":"space-between", "border":true, "background":false },
  "props": { "variant":"title|caption", "onPress":<fn>, "value":50, "min":0, "max":100 },
  "text": "...", "children": [ ... ] }
```

- **JS**: `import { View, Text, Submenu, Dialog } from "aether/ui"` → reconciler
  hasilkan node-desc → `reify()` → `UiNode` (jalan sekarang).
- **WASM**: interface `aether:ui` (crate Rust / header C ter-generate) bangun
  node-desc lewat host call.
- **C built-in**: `#include <aether/ui.h>` → panggil builder `widgets.h` langsung
  (tanpa serialisasi).

Field manifest `display_server: aether` (Plan 50/59) men-wire SDK ini saat launch.

### Contoh node-tree (tier 2, idiom Submenu)

```cpp
// App: layar pengaturan WiFi — Submenu + Dialog konfirmasi.
UiNode* build(NodeArena& a, AppState& st) {
    return Col(a, {.padding=2}, {
        Header(a, "Wi-Fi"),
        Submenu(a, st.menu, {                       // modul Flipper, state persisten
            ListRow(a, "Scan",    onScan,    &st),
            ListRow(a, "Forget",  onForget,  &st),
            Toggle (a, "Enabled", st.on, onToggle, &st),
        }),
        Footer(a, rt.input().hintFor(Action::Activate)),  // hint, bukan "OK" hardcoded
    });
}
UiNode* buildModal(NodeArena& a, AppState& st) {        // dipanggil saat st.confirming
    return Dialog(a, { .icon=icon("action.warning"), .title="Forget network?",
                       .left="Cancel", .right="Forget",
                       .onResult=onForgetResult, .ctx=&st });
}
```

---

## Fase

- [ ] **Fase 1 — Namespace + tier 0/1 toolkit.** Bungkus UI core ke `aether::ui`;
      angkat `text_style.*` + ekstrak helper dari `renderer.cpp`/`elements`-ala-Flipper
      jadi `aether::ui::draw` (text/icon/frame/box/separator/scrollbar/multiline).
      Tier 0 = `Surface::canvas()` (Canvas + blitRgb565). Renderer node baca toolkit.
      Host/WASM: parity visual dgn sekarang.
- [ ] **Fase 2 — Renderer pixelete.** Upgrade `paint()` (`renderer.cpp:7`): frame
      rounded, separator, scrollbar bergaya, fokus ring crisp, marquee teks panjang.
      Semua baca Theme (Plan 53). Uji snapshot host.
- [ ] **Fase 3 — Modul widget Flipper.** Tambah builder + state: Submenu, Dialog,
      Popup, List (VariableItemList), Loading; promosikan `TextInput`. Tiap modul
      uji layout + fokus + dispatch di host.
- [ ] **Fase 4 — Compositor + Surface.** Ubah `AetherServer::renderFrame` jadi
      `Compositor::composite`; status bar = surface persist, app = surface
      foreground; raw surface (game/kamera) via blitRgb565. Kebijakan single-foreground.
- [ ] **Fase 5 — Ekspos `aether:ui` ke 3 runtime.** Generator (Plan 49) + import
      (Plan 50): JS `aether/ui` (perluas `reify` untuk modul baru), header C
      `<aether/ui.h>`, interface WASM. Wire via manifest `display_server`.
- [ ] **Fase 6 — Komponen krusial + responsive/scale.** `ListView`/`SmartLabel`/
      `FileBrowser` (Row+ScrollView + `draw::ellipsis`/`marquee`/`scrollbar` dashed);
      `MainMenu` carousel DSi (`draw::banner`/`posbar` + Row tile, ikon Plan 53).
      Responsive: base logical res + scale auto + fit mode (`fill`/`letterbox`),
      lanjutan Plan 25. Ikon app dari registry+manifest (Plan 59). Snapshot host
      tiap komponen di beberapa logical size.

**Build/uji:** host + WASM tiap fase; ESP32 build-only Fase 2, 4 & 6.

---

## File yang disentuh

**Core UI (dipertahankan + diperluas):**
- `firmware/core/include/nema/ui/node.h` — model node (tak berubah; mungkin
  `NodeType` tambah varian untuk modul yang butuh node native).
- `firmware/core/include/nema/ui/widgets.h` · `src/ui/widgets.cpp` — builder modul
  Flipper baru (Submenu/Dialog/Popup/List/Loading) + state struct.
- `firmware/core/include/nema/ui/renderer.h` · `src/ui/renderer.cpp` — **renderer
  pixelete** (rewrite paint, baca Theme).
- `firmware/core/include/nema/ui/text_style.h` · `src/ui/text_style.cpp` — diangkat
  jadi bagian tier-1 toolkit.
- `firmware/core/src/ui/layout.cpp` — penyesuaian kecil bila modul perlu.
- `firmware/core/include/nema/ui/component_runtime.h` · `.cpp` — dispatch modul baru.
- `firmware/core/include/nema/ui/canvas.h` · `src/ui/canvas.cpp` — tier 0 (sudah ada).

**Baru:**
- `firmware/core/include/nema/ui/draw_toolkit.h` · `src/ui/draw_toolkit.cpp` —
  tier-1 `aether::ui::draw` (frame/box/separator/scrollbar/multiline/marquee/icon).
- `firmware/core/include/nema/ui/surface.h` · `src/ui/surface.cpp` — `Surface`.
- `firmware/core/include/nema/ui/compositor.h` · `src/ui/compositor.cpp` — `Compositor`.
- `firmware/core/include/nema/ui/modules/` — submenu/dialog/popup/list/loading
  (builder + state), promosi `text_input.*`/`virtual_keyboard.*`.
- `firmware/core/include/nema/ui/components/` — komponen krusial:
  `list_view.*`, `smart_label.*` (fokus+lebar → biasa/ellipsis/marquee),
  `file_browser.*` (resolve ikon per-ekstensi), `main_menu.*` (carousel DSi +
  `MainMenuState`). Dibangun dari `widgets.h` + `draw_toolkit.h`.
- `firmware/core/include/nema/ui/responsive.h` · `src/ui/responsive.cpp` — base
  logical res, scale auto (`logical ≥ base`), fit mode (`fill`/`letterbox`/`stretch`);
  baca `IDisplayDriver::dpi()` (Plan 25) + `Theme.scale` (Plan 53).

**Backend / binding:**
- `firmware/core/src/ui/aether_server.cpp` — drive compositor.
- `firmware/core/src/js/js_engine.cpp` — `reify()` modul baru.
- SDK generator (Plan 49) + header `<aether/ui.h>` (Plan 50).

**Plan 53** mendefinisikan `Theme`/`AssetPack` yang dibaca tier-1 + renderer.

---

## Test

- **Host unit (layout/widget):** perluas test layout flexbox; test bentuk node tiap
  modul baru (Submenu/Dialog/Popup/List) — struktur tree + hasil `layout()`.
- **Host snapshot renderer:** render tree ke buffer 1-bit, bandingkan PNG/ASCII
  referensi (Fase 1 parity = pixel-identik dgn renderer lama; Fase 2 = baseline
  pixelete baru). Pakai `ascii_board_renderer` yang sudah ada untuk dump.
- **Host dispatch:** simulasikan `dispatchNav`/`dispatchPointer` pada tiap modul —
  fokus berpindah, Activate memanggil callback, tap-vs-drag, momentum.
- **Compositor:** surface status-bar persist saat app surface berganti; z-order;
  single-foreground policy; raw surface tak terhapus toolkit.
- **WASM:** muat app JS contoh (Submenu+Dialog) lewat `reify`, verifikasi node-tree
  & redraw flag (regresi "app blank" yg sudah difix).
- **Komponen krusial:** ListView baris = Row(ikon|label flexGrow|aksesori) + scrollbar
  dashed; SmartLabel pilih biasa/ellipsis/marquee menurut fokus+lebar; FileBrowser
  resolve ikon per-ekstensi (`file.folder`/`file.txt`/`file.generic`); MainMenu
  carousel — tile terpilih di tengah, notch banner ikut, posbar segmen+dashed.
- **Responsive/scale:** scale auto = integer terbesar `logical ≥ base` (256×128→2,
  320×240→2); reflow tambah baris ListView di logical besar; `letterbox` jaga rasio
  base + center; ikon/font integer-scale crisp (nearest-neighbor).
- **ESP32 build-only:** Fase 2, 4 & 6 — kompilasi bersih, tak ada heap per-frame
  (NodeArena), cek ukuran flash/RAM.

---

## Risiko & mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Renderer rewrite ubah visual semua layar sekaligus | Regresi diam-diam | Fase 1 = parity wajib (snapshot pixel-identik) sebelum Fase 2 gaya baru |
| Modul Flipper "view" itu immediate-mode; dipaksa ke node-tree bisa janggal | Modul kompleks (file_browser) sulit | Port hanya 6 modul inti sbg node+state; modul berat ditunda (non-goal) |
| `NodeArena` fixed-pool kehabisan saat tree modul besar | Node `nullptr` → UI rusak | Ukuran arena per-surface configurable; assert + `rt.log().error` saat exhausted |
| Compositor menambah overhead per-frame di MCU | FPS turun | v1 single-foreground = komposit 2 surface saja; dirty-rect menyusul bila perlu |
| Tema dibaca renderer (Plan 53) belum siap saat Fase 2 | Blok | Sediakan `Theme` default statis di Plan 52; Plan 53 isi pack/persistence |
| Binding 3-runtime drift dari node-desc | SDK tak konsisten | Node-desc = sumber kebenaran tunggal; generator (Plan 49) hasilkan semua dari satu IDL |
| Multi-size font belum ada (v1 FONT_5X8 monospace) | Teks besar kasar | Diterima v1; **format `Font` proporsional + `.afont` sudah dikunci di Plan 53** → tambah font asli = isi data + loader, tanpa ubah arsitektur |
| Marquee SmartLabel = tick per-frame di banyak baris | CPU/redraw boros | Marquee hanya baris **fokus** (satu); sisa = ellipsis statis; tick reuse `tickMomentum` (`component_runtime.h:66`) |
| MainMenu carousel (chrome banner/notch/posbar) di luar node-tree flexbox | Chrome custom sulit & off-grid | Tile = `Row` node-tree (dapat layout/fokus); hanya chrome (banner/notch/posbar) lewat `draw::*` tier-1 — terpisah rapi |
| Fit `stretch` merusak crisp pixelete | Visual jelek | Default `fill` (reflow); `letterbox` jaga rasio base; `stretch` ada tapi ditandai "tidak disarankan" (board/settings, bukan default) |
| Base logical terlalu besar utk display kelas Flipper | Konten terpotong | Base = lantai 128×64; scale auto jamin `logical ≥ base`; reflow tambah baris di display besar, tak memotong di base |
