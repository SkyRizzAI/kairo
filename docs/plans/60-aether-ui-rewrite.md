# 60 — Aether UI Total Rewrite (Flipper/DSi, board-agnostic)

> Rewrite total tampilan **Aether** mengikuti gaya **Flipper Zero Momentum**
> (`refs/flipper-zero-momentum-firmware/`) + **menu carousel ala Nintendo DSi**,
> tanpa mengorbankan prinsip **board-agnostic** (reflow, bukan zoom).

- Status: ✅ Implemented — Fase 0–6 complete: draw toolkit (draw.h/draw.cpp), renderer rewrite, TitleBar/ListItem/Toggle widgets, StatusBar battery icon, DSi HomeScreen carousel, LockScreen clock, all settings screens converted to TitleBar + ListItem; theme cycling in SleepSettingsScreen; LogsScreen with logForEach callback; builds pass (host + WASM)
- Milestone: M13 (App Platform) — UI overhaul
- Depends on: Plan 50 (UI SDK model), 51 (display server), **52 (Aether UI SDK — SUMBER DESAIN)**, 53 (theming/StyleTokens), 55 (surface/ISurface)
- **Supersedes:** pendekatan *incremental + parity* di Plan 52 (Fase 1 "pixel-identik")
  → diganti **big-bang total rewrite**. Anatomi komponen di Plan 52 tetap jadi
  referensi desain; Plan 60 adalah rencana eksekusinya.

---

## 0. Konteks & keputusan (dikunci)

- **Pendekatan: BIG-BANG TOTAL.** Fokus merombak Aether sepenuhnya — tier-1 draw
  toolkit + tier-2 widget + renderer + semua screen ditulis ulang sekaligus.
  Bukan migrasi bertahap, bukan parity dengan tampilan lama.
- **Backup, bukan hapus.** Seluruh UI Aether lama dipindah ke
  `firmware/.bak/aether-v1/` (di luar source tree, tidak ikut di-build). Tetap
  bisa dirujuk/dikembalikan.
- **Referensi visual:** Flipper Momentum (`elements.c`, submenu, dialog, marquee)
  + carousel DSi (banner + notch ▼, tile rounded, position bar).
- **Board-agnostic tetap mutlak:** semua baca `canvas.width()/height()`, base
  logical resolution, **reflow** (layar besar = lebih banyak baris, bukan baris
  membesar), fit modes (fill/letterbox). Tidak ada cabang per-nama-board.
- **Theming dari Plan 53:** semua warna/spacing/font/ikon lewat `StyleTokens`
  (`theme()`), sudah ter-wire ke `fontForRole()`. Ganti tema = seluruh UI ikut.

---

## 1. Status saat ini — apa yang SUDAH vs BELUM berubah

Menjawab langsung: *"system-nya dan UI layer-2-nya sudah berubah belum?"*

| Lapisan | Status | Catatan |
|---|---|---|
| **System / plumbing** (Plan 48–59) | ✅ **SUDAH** | IDL codegen, `RuntimeTier`, display-server negotiation, PAPP1 loader, `ISurface`/`AppContext`, `StyleTokens`+`theme()`, `config` CLI, `ps` tier display, `aether:ui` ABI + host impl. Ini *non-visual* — pipa, bukan tampilan. |
| **tier-0 primitif engine** (`canvas`, `font_5x8`, `node`, `layout`, `component_runtime`) | ✅ ada, matang | **DIPERTAHANKAN** (node-tree + flexbox sudah bagus). Tidak dibuang. |
| **tier-1 drawing toolkit** (`aether::ui::draw`) | ❌ **BELUM ADA** | Didesain di Plan 52, nol implementasi. Inti rewrite. |
| **tier-2 widget/komponen** (look) | ❌ **BELUM berubah** | Masih `widgets.h` lama (ListRow datar, Button border kotak). Komponen DSi/Flipper (ListView, MainMenu, SmartLabel, FileBrowser, dashed scrollbar) belum ada. |
| **renderer** (`renderer.cpp` paint) | ❌ masih lama | Frame/box kotak polos, belum pixelete/rounded/ber-tema. |
| **screens** (13 layar) | ❌ masih lama | Semua pakai `ListRow` flat. Ditulis ulang ke komponen baru. |

**Kesimpulan:** *sistem* (pipa) sudah; *tampilan* (tier-1/2 + renderer + screens)
belum sama sekali — itulah scope Plan 60.

---

## 2. Arsitektur baru — 3 tier (di dalam SDK Aether)

```
┌─ Tier 2: node-tree deklaratif (DEFAULT) ───────────────────────────┐
│   widgets baru + KOMPONEN krusial (ListView, MainMenu, SmartLabel,  │
│   FileBrowser, Dialog…) → UiNode* tree, dapat layout + fokus gratis │
├─ Tier 1: drawing toolkit ber-tema  (aether::ui::draw) ─────────────┤
│   pixelete ala elements.c: frame, box(rounded), separator,          │
│   scrollbar(dashed, V/H), multiline, marquee, ellipsis, icon,       │
│   banner(+notch), posbar. Baca StyleTokens (Plan 53). SEMUA paint   │
│   resmi lewat sini — renderer node DAN raw-draw screen.             │
├─ Tier 0: primitif engine (DIPERTAHANKAN) ─────────────────────────┤
│   Canvas (1-bit), font_5x8, NodeArena/UiNode, layout(flexbox),      │
│   component_runtime (focus/scroll/momentum)                         │
└────────────────────────────────────────────────────────────────────┘
```

Prinsip: **tier-2 dipakai screen normal** (deklaratif, otomatis layout+fokus);
**tier-1 dipakai chrome** yang off-grid (carousel banner/notch/posbar) dan oleh
renderer untuk menggambar node. Tier-0 tidak disentuh.

---

## 3. Strategi backup (.bak)

```
firmware/.bak/aether-v1/          # snapshot UI lama (di luar build tree)
  ui/                             # src/ui/* yang di-rewrite
    aether_server.cpp  renderer.cpp  widgets.cpp  status_bar.cpp
    components.cpp  (look lama)
  ui-include/                     # include/nema/ui/* yang di-rewrite
    widgets.h  renderer.h  status_bar.h  components.h
  screens/                        # SEMUA src/screens/* lama
  screens-include/                # include/nema/screens/* lama
  README.md                       # kenapa di-backup, cara restore
```

- **Dipindah (di-rewrite):** `aether_server`, `renderer`, `widgets`,
  `status_bar`, `components`, **semua screen**.
- **TIDAK dipindah (tier-0, dipakai ulang):** `canvas`, `font_5x8`, `node`,
  `layout`, `component_runtime`, `component_screen`, `focus`, `hit_test`,
  `text_style`, `style_tokens`, `surface`, `display_server`, `ui_constants`,
  `view_dispatcher`, `text_input`, `virtual_keyboard`, `aether_abi`, `ui_sdk`,
  `ascii_board_renderer`, `fbcon_server` (server teks terpisah, tak tersentuh).
- CMake `NEMA_CORE_SRCS` daftar eksplisit (bukan glob) → file di `.bak/` aman
  tak ikut compile.

---

## 4. Katalog pages / screens (yang akan dipakai)

Semua di-rewrite ke komponen baru. Tanda 🔁 = ganti bentuk, ➕ = baru, ⛔ = dibuang.

### A. Chrome / surface sistem
| Surface | Lama | Baru (Plan 60) |
|---|---|---|
| **StatusBar** | bar atas polos | 🔁 ber-tema (jam, wifi, batt, judul app) via tier-1 `draw` |
| **MainMenu (Home)** | `HomeScreen` Col of ListRow + "PALANU" | 🔁 **carousel DSi**: banner+notch ▼, tile rounded horizontal, posbar. Entri: Continue (app paused), Apps, Logs, Settings |
| **LockScreen** | "LOCKED" | 🔁 ber-tema, jam besar + hint unlock |
| **Modal/Dialog overlay** | box kotak | 🔁 `Dialog` ala Flipper (rounded box + judul + tombol) |

### B. Navigasi inti
| Screen | Lama | Baru |
|---|---|---|
| **AppListScreen** (launcher) | ScrollView ListRow | 🔁 **ListView** (ikon app + SmartLabel + accessory ▶ + dashed scrollbar) |
| **SettingsScreen** (root) | list capability-gated | 🔁 **ListView** (ikon + label + nilai/▶) |
| **LogsScreen** | "LOGS" scroll teks | 🔁 ListView padat (level-tag warna/ikon) + auto-scroll |

### C. Settings sub-screens
| Screen | Isi | Baru |
|---|---|---|
| **DisplaySettings** (`sleep_settings`) | sleep/lock/fps | 🔁 ListView + Stepper/Toggle. ➕ tambah **Theme** (default/compact/large) di sini |
| **ControlsScreen** | input/keymap | 🔁 ListView |
| **TouchSettings** | kalibrasi/toggle touch | 🔁 ListView + Toggle |
| **SoundsSettings** | input/output audio | 🔁 ListView + Select |
| **CameraSettings** | param kamera | 🔁 ListView (atau "No camera hardware" empty-state) |
| **ProfileSettings** | user/device/password | 🔁 ListView + TextField |
| **AboutScreen** | info firmware | 🔁 panel info ber-tema (versi, board, build) |

### D. Apps built-in (di-rewrite pakai komponen baru)
| App | Baru |
|---|---|
| **ClockApp** | jam besar center, ber-tema |
| **StopwatchApp** | timer + tombol start/lap |
| **CounterApp** | demo stepper |
| **TickerApp** | marquee demo (pakai `draw::marquee`) |
| **TaskDemoApp** | progress/list async |
| **TouchTestApp** | canvas raw (tier-0/1) |
| **UiShowcaseApp** | 🔁 **galeri komponen baru** (jadi acuan visual & test) |
| **CameraApp** | viewfinder fullscreen (raw RGB565, sudah ada) |
| **WifiApp / BluetoothApp** | ListView scan + status |
| **JsApp** (runtime) | render node-tree via `aether:ui` — sudah jalur ABI |

### E. Dibuang / digabung
- ⛔ `close_and_open_modal.cpp` (demo modal lama) → digantikan `Dialog` di UiShowcase.
- Pertimbangkan gabung **AppList ke MainMenu** (carousel = launcher utama, ala DSi)
  — *keputusan saat Fase 4* (lihat Risiko).

---

## 5. Komponen baru (anatomi → detail di Plan 52 §"Komponen krusial")

| Komponen | Bentuk singkat |
|---|---|
| **ListView** | `Row`(ikon 10×10 \| `SmartLabel` flexGrow \| accessory) + `draw::scrollbar` dashed vertikal |
| **SmartLabel** | text-aware: normal → ellipsis (panjang+unfocused) → marquee (panjang+focused) |
| **FileBrowser** | varian ListView, ikon per-ekstensi (`icon.folder/.txt/.generic`) |
| **MainMenu** | carousel DSi: `draw::banner`(+notch) + tile `Row` node-tree + `draw::posbar` + scrollbar horizontal |
| **Dialog** | rounded box + judul + body + `Row` tombol (Flipper-style) |
| **dashed scrollbar** | tier-1 primitive, V (list) + H (carousel), thumb solid di atas track dashed |

Tier-1 `aether::ui::draw` (baru): `frame`, `box(rounded)`, `separator`,
`scrollbar(horizontal=bool)`, `multiline`, `marquee`, `ellipsis`, `icon`,
`banner(notch)`, `posbar`. Semua baca `theme()` (StyleTokens, Plan 53).

---

## 6. Board-agnostic / responsive (wajib)

- **Base logical resolution** (mis. 128×64) = lantai desain. Komponen baca
  `canvas.width()/height()`, **tak pernah** hardcode dimensi.
- **Scale auto:** integer terbesar yang menjaga logical ≥ base (256×128 → scale 2).
- **Reflow, bukan zoom:** layar lebih besar → lebih banyak baris ListView terlihat,
  bukan baris membesar.
- **Fit modes:** `fill` (default, reflow penuh), `letterbox` (jaga rasio + center),
  `stretch` (tidak disarankan). Board deklarasi resolusi+DPI+fit default.
- Ikon & font integer-scale (nearest-neighbor) → crisp di semua board.
- Skala visual (spacing/font/ikon) dari `theme()` → `compact`/`default`/`large`
  sudah ada (Plan 53).

---

## 7. Fase eksekusi (big-bang)

> Urutan agar firmware kembali bisa di-build secepat mungkin setelah "mati" awal.

- [x] **Fase 0 — Backup.** `firmware/.bak/aether-v1/` + README.
- [x] **Fase 1 — Tier-1 draw toolkit.** `nema/ui/draw.h` + `src/ui/draw.cpp` (`aether::ui::draw::*`).
- [x] **Fase 2 — Renderer pixelete.** `renderer.cpp` rewritten with box_rounded, scrollbar dashed, inverted text.
- [x] **Fase 3 — Komponen tier-2.** `TitleBar`, `ListItem`, `Toggle` in `widgets.h`; `components.cpp` drawModalBox/drawConfirm rounded; `status_bar.cpp` with battery icon + inverted band.
- [x] **Fase 4 — Chrome + screens inti.** DSi HomeScreen carousel (banner + center/side tiles + posbar); AppList/Settings/Logs with TitleBar+ListItem; logForEach callback API.
- [x] **Fase 5 — Settings sub + apps.** Display/Controls/Touch/Sounds/Camera/Profile/About + theme cycling (SleepSettingsScreen).
- [x] **Fase 6 — Lock/Dialog + polish.** LockScreen wall-clock + board-agnostic unlock hint; all screens use TitleBar; builds pass host + WASM.

**Build/uji:** host + WASM tiap fase; ESP32 build-only di Fase 2, 4, 6.

---

## 8. Struktur file baru

```
firmware/core/include/nema/ui/
  draw.h                    ➕ tier-1 toolkit
  widgets.h                 🔁 idiom tier-2 baru
  renderer.h                🔁
  status_bar.h              🔁
  components/               ➕ komponen krusial
    list_view.h  smart_label.h  file_browser.h  main_menu.h  dialog.h
  responsive.h              ➕ scale/fit (board-agnostic helper)

firmware/core/src/ui/
  draw.cpp  renderer.cpp  widgets.cpp  status_bar.cpp  aether_server.cpp  responsive.cpp
  components/  (list_view.cpp  smart_label.cpp  file_browser.cpp  main_menu.cpp  dialog.cpp)

firmware/core/src/screens/   🔁 semua di-rewrite
firmware/.bak/aether-v1/     📦 snapshot lama
```

Update `firmware/core/CMakeLists.txt` `NEMA_CORE_SRCS`: buang yang ke `.bak`,
tambah `draw.cpp`, `responsive.cpp`, `components/*.cpp`.

---

## 9. Test

- **Snapshot host** tiap komponen di beberapa logical size (128×64, 256×128) →
  verifikasi reflow (lebih banyak baris, bukan membesar).
- **Tema:** render set sama di `compact`/`default`/`large` → spacing/font/ikon ikut.
- **SmartLabel:** 3 state (fit / ellipsis / marquee) sesuai fokus+lebar.
- **MainMenu carousel:** banner/notch/posbar align ke tile terpilih.
- **Tidak ada heap per-frame** (NodeArena reset only) — cek di ESP32 build.
- **Navigasi floor:** Prev/Next/Activate/Back tetap reachable (validateFloor).

---

## 10. Risiko & mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Big-bang → firmware "mati" sementara | Tak bisa boot di tengah | Backup utuh di `.bak`; Fase 4 target boot-able lagi; commit per fase |
| MainMenu carousel off-grid (banner/notch/posbar) | Chrome custom sulit | Tile = `Row` node-tree (layout+fokus gratis); hanya chrome lewat `draw::*` tier-1 |
| Theme/StyleTokens belum lengkap saat Fase 2 | Blok | `theme()` default statis sudah ada (Plan 53 wired) |
| AppList vs MainMenu tumpang tindih | Bingung navigasi | Putuskan di Fase 4: carousel = launcher utama ala DSi, atau Apps tetap sub-list |
| Regresi visual diam-diam | UI rusak tak ketahuan | Snapshot host wajib tiap komponen sebelum lanjut fase |
| Board kecil (1-bit, low-res) vs target besar | Layout pecah | Responsive `fill`+reflow; uji 128×64 sebagai lantai |

---

## 11. Yang TIDAK berubah (biar jelas)

- Tier-0 engine (canvas/font/node/layout/component_runtime) — dipakai ulang.
- `fbcon_server` (console TTY) — backend terpisah, tak tersentuh.
- System API / IDL / runtime tier / PAPP1 / `aether:ui` ABI — sudah selesai (Plan 48–59).
- Model navigasi `input::Action` + `IKeyMap` board — tetap (UI baca Action, bukan tombol fisik).
