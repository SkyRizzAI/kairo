# Plan 86 — Process-First App Model (Node/Electron-style)

> **Status:** PLANNING — belum mulai implementasi.
> **Last updated:** 2026-06-22

---

## 0. Cara pakai dokumen ini (WAJIB dibaca AI Builder)

Dokumen ini adalah **kontrak**. Tujuannya mencegah builder berhalusinasi atau
melenceng dari goal. Aturan main:

1. Sebelum mulai fase apa pun: baca `docs/STATE.md`, lalu bagian **§1 Goals**,
   **§4 Arsitektur target**, dan **§5 ABI spec** di sini.
2. **Jangan menambah fitur di luar fase yang sedang dikerjakan.** Kalau ketemu ide
   baru, catat di bagian **§11 Backlog**, jangan langsung implementasi.
3. Setiap nama fungsi/method/struct yang kamu pakai harus **sudah ada** (lihat §3
   file:line) atau **didefinisikan eksplisit di sini** (§5). Kalau bukan keduanya,
   berarti kamu berhalusinasi — STOP, verifikasi dulu dengan baca file aslinya.
4. Selesai tiap fase: jalankan **success criteria** fase itu (§6), centang checklist
   (§9), update docs sesuai **Definition of Done** (§10).
5. **Invariants (§2) tidak boleh dilanggar** di fase mana pun.

---

## 1. Goals

### 1.1 Goal utama

Satukan model app Palanu (built-in, WASM, JSX/TSX) di bawah satu mental model:
**setiap app adalah proses**, seperti Node.js. Default jalan di terminal
(stdin/stdout/stderr + argv). Kalau app mau GUI, dia **memanggil** fungsi UI
(analogi `require('electron')`) untuk membuka window Aether — keputusan ada di
**app saat runtime**, bukan di launcher saat manifest dibaca.

Analogi acuan (pegang sampai verifikasi akhir):

> Seperti Node.js: app ditulis dalam "bahasa terminal", bisa dijalankan lewat CLI,
> bisa dijalankan lewat .papp bundle dengan bantuan manifest — persis shortcut
> desktop Linux yang sebenarnya mengarah ke CLI tapi dengan args/parameter untuk
> mode UI.

### 1.2 Satu app harus bisa (definisi sukses akhir)

- [G1] **Ambil input terminal** (stdin) seperti app CLI biasa.
- [G2] **Keluarkan output** (stdout/stderr) seperti CLI biasa.
- [G3] **Terima args/params**, contoh `counter inc`, `hello --name Budi`.
- [G4] **Memanggil init window UI retained** (Aether) — dapat focus-nav, layout,
  scroll, theming otomatis.
- [G5] **Atau ambil raw canvas** untuk gambar pixel sendiri (game/visualisasi).
- [G6] App yang **cuma `printf`** (tanpa panggil UI) → tampil di **terminal screen**
  di device, tidak langsung hilang.
- [G7] Di-launch dari **icon** = `run <app>` + `manifest.args` (analogi shortcut).
- [G8] **`.papp.zip`** di-upload lewat Forge → **auto-unpack** → app muncul di Apps.
- [G9] **DX simpel** (acuan AkiraOS): `printf`, `display_*`, single header.

### 1.3 Non-goals (JANGAN dikerjakan di plan ini)

- Multi-window / window manager. Satu app = satu surface foreground.
- Display server selain `aether` (fbcon menyusul, di luar scope).
- Preemptive multitasking antar app. Tetap single-foreground (Plan 84).
- Konvergensi penuh JS/TSX ke ABI baru (cukup didefinisikan, impl menyusul — §6 Fase 8).
- WASI libc. Tetap bare-metal `wasm32-unknown-unknown` (Plan 85).

---

## 2. Invariants (tidak boleh dilanggar)

| ID | Invariant | Sumber |
|----|-----------|--------|
| I1 | Semua logging app/sistem lewat `rt.log()`. Tidak ada `printf`/`Serial`/`std::cout` untuk logging firmware. (`nema_print` guest ≠ logging firmware — itu stdout app.) | CLAUDE.md |
| I2 | UI tidak boleh hardcode dimensi layar. Pakai `canvas.width()/height()` (sudah logical). Contoh app pun harus mendemokan ini. | CLAUDE.md |
| I3 | Cek kapabilitas (`rt.capabilities().has(...)`), bukan tipe board / `#ifdef`. | CLAUDE.md |
| I4 | **Launcher tidak pernah memutuskan UI vs CLI.** Launcher hanya menjalankan proses; app yang memutuskan. | Plan ini |
| I5 | **Terminal-by-default.** App stdout-only HARUS tampil di terminal screen, tidak boleh langsung exit/blank. | G6 |
| I6 | Warna canvas = 1-bit `bool on`. `COLOR_BLACK`→`on=false`, `COLOR_WHITE`→`on=true`. Tidak ada warna lain. | `canvas.h` |
| I7 | Tidak ada host→guest callback untuk event UI. App pegang loop sendiri & tarik event via `ui_wait_event()`/`input_wait()`. (Hindari kerapuhan re-entrancy wasm3.) | §5.3 |
| I8 | WASM tetap bare-metal: `-nostdlib --target=wasm32-unknown-unknown`. Tidak boleh re-introduce WASI libc. | Plan 85 |

---

## 3. Kondisi sekarang (riset — file:line, untuk grounding)

**Sudah ada (jangan bikin ulang):**

- `AppContext : public ISurface, public ProcessContext`
  — `firmware/core/include/nema/app/app_context.h:18`. Satu app **sudah** punya
  `args()/in()/out()/err()/requestExit()` **dan** `canvas()/present()/nextInput()`.
- `ISurface` — `firmware/core/include/nema/ui/surface.h:32` :
  `canvas()`, `present()`, `nextInput(InputEvent&)`, `waitInput(InputEvent&, timeoutMs)`.
- `Canvas` — `firmware/core/include/nema/ui/canvas.h:71` . Method nyata:
  `width()`, `height()`, `clear(bool on=false)`, `drawPixel(x,y,on)`,
  `fillRect(x,y,w,h,on)`, `drawRect(x,y,w,h,on)`, `fillRoundRect(...)`,
  `drawRoundRect(...)`, `drawLine(x0,y0,x1,y1,on)`, `drawBitmap(x,y,w,h,bits)`,
  `invertRect(...)`, `setFont(...)`, `drawText(...)` (lanjutan header).
- `ProcessContext` — `firmware/core/include/nema/proc/process_context.h:15` :
  `args()`, `cwd()`, `env()`, `in()/out()/err()`, `requestExit()/shouldExit()/exitCode()`.
- WASM `main(argc, argv)` dipanggil — `firmware/core/src/wasm/wasm_engine.cpp:63-73`.
  WASI `args_get` suplai argv — `wasm_wasi.cpp:56-79`. `fd_write`→stdout,
  `fd_read`→stdin, `proc_exit` — `wasm_wasi.cpp:83-156`.
- `nema.*` host imports — `firmware/core/src/wasm/wasm_nema.cpp:190-202`
  (`print`, `log`, `device_name`, `device_caps`, `storage_fs_read_file/write_file`).
- `WasmHostCtx{ ctx, appId, printHook }` — `wasm_engine.h:24-30`.
- SDK header `nema_api.h` — `packages/app-sdk/include/nema_api.h`
  (`NEMA_IMPORT`/`NEMA_EXPORT`, minimal libc inline).
- CLI shell `CliService` — `firmware/core/src/services/cli_service.cpp` :
  tokenizer (`:44-62`), `run <app> args | pipes` (`:302-344`), auto-launch (`:65-113`).
- Launcher path: `app_list_screen.cpp:39-44` → `AppRegistry::launch(id)`
  (`app_registry.cpp:140-181`) → `AppHostManager::launch` / `launchProcess`.
- `.papp` format (folder + single-file PAPP1) — `papp_package.cpp`, `papp_installer.cpp`.
  Install scan: `loadInstalledPapps()` (`papp_installer.cpp:204-251`).
- SDK build → `dist/<id>.papp/` — `packages/app-sdk/bin/build.ts`.
- Forge VFS write — `packages/forge/src/lib/RemoteSession.ts:442-452` (`writeFile`).

**Salah / kurang (yang diperbaiki plan ini):**

1. `mode: "cli"|"ui"|"hybrid"` memaksa keputusan di muka —
   `papp_installer.cpp:116-119`, `app_registry.cpp` (cabang `AppMode::Cli`). → **hapus**.
2. **Tidak ada UI ABI untuk WASM** (gap "Plan 84 Fase 4") — `aether_abi.h` host-side
   saja, belum jadi wasm imports (`wasm_app.h:17-19`). → **Fase 2 & 3**.
3. Launcher `launch(id)` tak meneruskan argv (`app_host.h:116` `args_` kosong). → **Fase 1**.
4. `WasmApp` jalan synchronous di `onStart()` lalu capture stdout — mencampur "render
   output" dengan "app". → **Fase 1** (jalankan `main()` di app thread).
5. `.papp.zip` + auto-unpack belum ada. → **Fase 6**.

---

## 4. Arsitektur target

### 4.1 Lifecycle (process-first)

```
launch(id, argv)                 // dari icon ATAU CLI `run`
   │
   ▼
UnifiedHost (punya AppContext = ISurface + ProcessContext)
   │  spawn app thread → main(argc, argv)
   │
   ├── Mode::Terminal  (DEFAULT)
   │     stdout/stderr (fd_write) → TerminalSurface → auto-render screen
   │     "Press any key to exit" saat main() return
   │
   └── Mode::Gui   (di-trigger panggilan ui_* ATAU canvas_* pertama)
         app pegang canvas; host stop auto-render terminal
         app loop sendiri, tarik event via ui_wait_event()/input_wait()
   │
   ▼
main() return / proc_exit  → host pop view, balik ke pemanggil
```

**Aturan transisi:** host mulai di `Mode::Terminal`. Panggilan pertama ke import
`ui.*` **atau** `canvas.*` mem-flip ke `Mode::Gui` (irreversible untuk run itu).
Sebelum flip, isi canvas tidak ditampilkan (yang tampil = terminal). Sesudah flip,
terminal buffer disembunyikan, canvas app yang tampil.

### 4.2 Manifest schema baru

```jsonc
{
  "id":        "com.palanu.example.counter",   // WAJIB
  "name":      "Counter",                       // WAJIB (fallback: id)
  "version":   "1.0.0",                          // WAJIB
  "entry":     "counter.wasm",                   // file entry (per runtime)
  "runtime":   "wasm",                           // "wasm" | "js" (default "js")
  "args":      ["--ui"],                         // BARU: default argv saat launch dari icon
  "display_server": "aether",                    // opsional; default any
  "icon":      "icon.raw",                        // opsional
  "category":  "Demo",                            // opsional
  "needs":     [],                                // opsional (kapabilitas)
  "api_version": "1.0"                            // opsional
  // "mode"  → DIHAPUS. Kalau ada di manifest lama: diabaikan (parse-tolerant).
}
```

- `args` = analogi `Exec=counter --ui` di file `.desktop`. Saat launch dari icon,
  argv = `[id] + args`. Saat dari CLI, argv = apa yang user ketik.

---

## 5. ABI spec (kontrak teknis — sumber kebenaran)

Notasi signature wasm3 (`m3_LinkRawFunction`): `v`=void, `i`=i32, `*`=i32 ptr
(offset ke linear memory guest). String = offset ke C-string NUL-terminated; host
resolve via `readCStr` (`wasm_nema.cpp:34-43`).

Konstanta (didefinisikan di SDK header, §5.4):
`COLOR_BLACK=0`, `COLOR_WHITE=1`.

### 5.1 Module `canvas` (raw — Fase 2)

| Guest fn | Sig | Host mapping (`Canvas`) |
|----------|-----|-------------------------|
| `canvas_width()` → i32 | `i()` | `canvas.width()` |
| `canvas_height()` → i32 | `i()` | `canvas.height()` |
| `canvas_clear(color)` | `v(i)` | `canvas.clear(color!=0)` |
| `canvas_pixel(x,y,color)` | `v(iii)` | `canvas.drawPixel(x,y,color!=0)` |
| `canvas_fill_rect(x,y,w,h,color)` | `v(iiiii)` | `canvas.fillRect(...)` |
| `canvas_rect(x,y,w,h,color)` | `v(iiiii)` | `canvas.drawRect(...)` |
| `canvas_line(x0,y0,x1,y1,color)` | `v(iiiii)` | `canvas.drawLine(...)` |
| `canvas_text(x,y,msg,color)` | `v(ii*i)` | `canvas.drawText(x,y, str, on)` |
| `canvas_flush()` | `v()` | `present()` (+ flip ke Gui mode) |

Panggilan **mana pun** di module `canvas` → host flip `Mode::Gui` (lazy, di call
pertama). `canvas_flush()` = `present()`.

### 5.2 Module `ui` (retained — Fase 3)

Model: app deklarasikan tree antara `ui_begin()`..`ui_end()`, lalu **app sendiri**
yang loop & tarik event. **Tidak ada host→guest callback** (Invariant I7).

| Guest fn | Sig | Semantik |
|----------|-----|----------|
| `ui_begin()` | `v()` | mulai frame baru (reset arena tree) |
| `ui_title(msg)` | `v(*)` | judul (TextRole::Title) |
| `ui_text(msg)` | `v(*)` | body text |
| `ui_button(label, id)` | `v(*i)` | tombol; `id`>0 dikembalikan saat aktif |
| `ui_row_begin()` / `ui_row_end()` | `v()` | container baris |
| `ui_col_begin()` / `ui_col_end()` | `v()` | container kolom |
| `ui_end()` | `v()` | commit + render frame (host jalankan focus-nav internal) |
| `ui_wait_event()` → i32 | `i()` | blok sampai user aktifkan widget; balikin `id`, atau `EV_BACK` |
| `ui_poll_event()` → i32 | `i()` | non-blocking; `EV_NONE` kalau tak ada |

Konstanta event: `EV_NONE=0`, `EV_BACK=-1`. `id` tombol = bilangan positif pilihan app.

Implementasi host (`wasm_ui.cpp`): `ui_*` membangun `UiNode` tree via `aether_abi.h`
builders ke `NodeArena`; `ui_end()` render + simpan map `node→id`; `ui_wait_event()`
jalankan satu putaran event loop (pakai pola `ComponentApp`: `waitInput` →
`navFromKey`/activate) dan kembalikan `id` widget yang di-Activate, atau `EV_BACK`
saat tombol Back. Panggilan `ui_*` pertama → flip `Mode::Gui`.

### 5.3 Module `input` + timing (Fase 4 — untuk app raw-canvas)

| Guest fn | Sig | Semantik |
|----------|-----|----------|
| `input_poll()` → i32 | `i()` | non-blocking; `ACT_NONE` kalau kosong |
| `input_wait(timeout_ms)` → i32 | `i(i)` | blok sampai input/timeout |
| `delay(ms)` | `v(i)` | yield app thread `ms` |

Konstanta action (map dari `input::Action`): `ACT_NONE=0`, `ACT_PREV=1`,
`ACT_NEXT=2`, `ACT_ACTIVATE=3`, `ACT_BACK=4`, `ACT_UP=5`, `ACT_DOWN=6`,
`ACT_LEFT=7`, `ACT_RIGHT=8`. Host translate `InputEvent`→Action lewat
`rt.input()` keymap (jangan kirim raw Key — Invariant: program ke Action).

### 5.4 SDK header (`packages/app-sdk/include/`) — DX (Fase 5)

- `nema_api.h` (umbrella) meng-`#include` `display.h` + `ui.h`, plus core lama
  (`nema_print`, `nema_log`, device, storage, minimal libc).
- **`printf(fmt, ...)` shim** (varargs → buffer → `nema_print`) — support minimal
  `%d %u %x %s %c %%`. Tanpa WASI libc. Supaya contoh sesimpel AkiraOS.
- Deklarasi `NEMA_IMPORT("canvas", ...)` & `NEMA_IMPORT("ui", ...)` &
  `NEMA_IMPORT("input", ...)` sesuai §5.1–5.3.
- Alias ergonomis (opsional, AkiraOS-style): `display_clear`→`canvas_clear`,
  `display_text`→`canvas_text`, `display_flush`→`canvas_flush`. (Boleh `#define`.)

Contoh target DX (harus bisa dikompilasi di Fase 7):

```c
#include "nema_api.h"

int main(int argc, char* argv[]) {
    printf("Hello from WASM! argc=%d\n", argc);   // → terminal kalau tak buka UI

    canvas_clear(COLOR_BLACK);
    canvas_text(10, 10, "Hello Palanu!", COLOR_WHITE);
    canvas_flush();                                // ← buka window (Mode::Gui)

    while (1) {
        int a = input_wait(1000);
        if (a == ACT_BACK) break;
    }
    return 0;
}
```

---

## 6. Fase per fase (goal · langkah · file · success criteria)

### Fase 0 — Model & manifest cleanup
- **Goal:** schema bersih, `mode` hilang, `args` ada, model terdokumentasi.
- **Langkah:**
  1. `packages/app-sdk/src/manifest.ts`: hapus field `mode`, tambah `args?: string[]`.
  2. `firmware/core/include/nema/app/app_manifest.h`: hapus/`[[deprecated]]` `AppMode`;
     tambah `std::vector<std::string> args`.
  3. `papp_installer.cpp`: stop baca `"mode"`; baca `"args"`.
  4. `docs/feats/app-model.md` (BARU): tulis model process-first + lifecycle (§4).
- **Success:** build firmware host hijau; `13/13` host test pass; manifest lama
  ber-`mode` tetap ke-install tanpa error (diabaikan).

### Fase 1 — Unified host (terminal-by-default, GUI-on-request)
- **Goal:** satu jalur launch; stdout→terminal; argv mengalir; flip Terminal↔Gui.
- **Langkah:**
  1. Ekstrak terminal `WasmApp` jadi **`TerminalSurface`** reusable (render baris +
     footer "Press any key", reuse logika `wasm_app.cpp:88-124`).
  2. `AppHost`: tambah `enum Mode { Terminal, Gui }`; default `Terminal`; auto-render
     `out()` ke `TerminalSurface`; sediakan `enterGuiMode()` (dipanggil ABI canvas/ui).
  3. `AppRegistry::launch(id)`: hapus cabang `AppMode::Cli`; selalu lewat `AppHost`
     dengan `argv = {id} + manifest.args`.
  4. `WasmApp`: jalankan `main()` di **app thread** (bukan `onStart` synchronous) agar
     app bisa punya event loop.
- **Success (simulator):**
  - App `printf`-only (hello tanpa UI) → terminal screen muncul & **bertahan** sampai
    user tekan tombol (Invariant I5).
  - `run hello Budi` di CLI → output `Hello Budi` di CLI (G1–G3).
  - Launch hello dari icon → argv berisi `[id]`+`args` (log buktinya).

### Fase 2 — Raw canvas ABI (escape hatch — duluan, paling cepat)
- **Goal:** WASM bisa gambar pixel & buka window. Bukti "GUI-on-request".
- **Langkah:**
  1. `firmware/core/src/wasm/wasm_canvas.cpp` (BARU): impl §5.1, `linkCanvasImports(mod)`.
  2. `wasm_engine.cpp::runStart`: panggil `linkCanvasImports` (samping `linkNemaImports`).
  3. Call pertama canvas → `host.enterGuiMode()`. `canvas_flush()`→`present()`.
  4. `nema_api.h`: tambah deklarasi `canvas_*` + `COLOR_*`.
- **Success (simulator):** example raw-canvas menggambar teks+kotak; window tampil;
  `canvas_width()/height()` mengembalikan 264/176 (logical); `input_wait` Back → exit.

### Fase 3 — Retained UI ABI (jalur utama)
- **Goal:** WASM bikin UI retained (dapat focus-nav/layout/theming).
- **Langkah:**
  1. `firmware/core/src/wasm/wasm_ui.cpp` (BARU): impl §5.2 di atas `aether_abi.h`;
     `linkUiImports(mod)`; simpan map `node→id`; event loop pola `ComponentApp`.
  2. `wasm_engine.cpp`: link `ui` imports.
  3. `nema_api.h` → `ui.h`: deklarasi `ui_*` + `EV_*`.
- **Success (simulator):** example counter-UI: judul + count + tombol Inc/Dec;
  navigasi Prev/Next pindah fokus; Activate → `ui_wait_event` balikin id tombol →
  count berubah & re-render; Back → exit. Tidak ada host→guest callback (I7).

### Fase 4 — Input & timing GUI
- **Goal:** app raw-canvas dapat input & timing.
- **Langkah:** `wasm_input.cpp` (atau gabung `wasm_canvas.cpp`): `input_poll/wait`,
  `delay`; map `InputEvent`→Action via keymap; wire ke `waitInput/nextInput`.
- **Success:** example raw-canvas merespons Prev/Next/Activate/Back; `delay(1000)`
  tidak nge-freeze GUI thread (host tetap responsif).

### Fase 5 — SDK DX ergonomics
- **Goal:** DX sesimpel AkiraOS.
- **Langkah:** `printf` shim di header; split `display.h`/`ui.h` + umbrella
  `nema_api.h`; alias `display_*`; update `build.ts` `-I` kalau perlu.
- **Success:** contoh §5.4 compile bersih (`bun run app:build:<ex>`), output wasm
  < 5 KB (Plan 85 budget), jalan di simulator.

### Fase 6 — `.papp.zip` + Forge auto-unpack
- **Goal:** build hasilkan `.papp` + `.papp.zip`; upload zip → auto-unpack.
- **Langkah:**
  1. `build.ts`: setelah folder `dist/<id>.papp/`, zip jadi `dist/<id>.papp.zip`
     (pakai `fflate`).
  2. Forge: deteksi upload `*.papp.zip` → unzip client-side → `writeFile` tiap entry
     ke `/system/apps/<id>.papp/...` → trigger install scan (`loadInstalledPapps`).
- **Success (simulator):** upload `com.palanu.example.counter-wasm.papp.zip` → folder
  ter-ekstrak di VFS → "installed: Counter" di log → app muncul di Apps list, jalan.

### Fase 7 — Examples + verifikasi vs goals
- **Goal:** buktikan SEMUA goal G1–G9 di simulator.
- **Langkah:**
  1. `examples/hello-wasm`: `printf`-only (uji G1/G2/G6) + baca argv (G3).
  2. `examples/counter-wasm`: headless `counter inc` (persist) + buka UI retained
     kalau tanpa args / `--ui` (G3/G4).
  3. `examples/canvas-demo-wasm` (BARU): raw-canvas pixel/teks (G5).
- **Success:** tabel verifikasi §7 semua ✅.

### Fase 8 — Konvergensi JS/TSX + built-in (north-star)
- **Goal:** definisikan (dan sebagian impl) app JSX/TSX di model sama.
- **Langkah:** dokumentasikan di `docs/feats/app-model.md` bahwa app JSX/TSX = proses
  yang memanggil UI retained; map `build()` lama → model baru. Impl penuh boleh
  menyusul; minimal app JS lama tetap jalan (no regression).
- **Success:** app JS contoh (`examples/counter`) tetap jalan; doc menjelaskan jalur
  konvergensi tanpa breaking.

---

## 7. Test plan / parameter sukses keseluruhan

Dijalankan di **simulator** (Forge WASM) setelah Fase 7. Semua harus ✅:

| Goal | Skenario uji | Lulus jika |
|------|--------------|-----------|
| G1/G2 | `run hello` di CLI | output muncul di CLI terminal |
| G3 | `run hello Budi` & `run counter inc` | argv terbaca; counter persist antar run |
| G6 | Launch hello dari icon (no UI call) | terminal screen muncul & bertahan, tidak blank |
| G4 | Launch counter `--ui` dari icon | window retained; fokus pindah Prev/Next; Inc/Dec jalan; Back exit |
| G5 | Launch canvas-demo | pixel/teks tergambar; pakai `canvas_width/height` (bukan hardcode) |
| G7 | Cek argv saat icon-launch | argv = `[id] + manifest.args` (log) |
| G8 | Upload `.papp.zip` via Forge | auto-unpack → install → muncul di Apps → jalan |
| G9 | Build contoh | compile bersih, wasm < 5 KB, header tunggal |

Plus regресi: **13/13 host test** (`firmware/tests/`) tetap pass tiap fase.

---

## 8. Risiko & mitigasi

| Risiko | Mitigasi |
|--------|----------|
| Retained event loop lintas WASM rumit (re-entrancy) | App pegang loop; host hanya balikin `id` via `ui_wait_event` (I7). Tidak ada host→guest call. |
| `main()` synchronous vs GUI `while(1)` | Fase 1 pindahkan `main()` ke app thread; `present`/`waitInput` handshake sudah thread-safe (`AppHost`). |
| Breaking app `mode:"cli"` lama | Examples kita semua di-update; manifest `mode` diabaikan, default process-first. Breaking diterima (lihat §0/§11). |
| `fflate` ukuran/komplikasi di Forge | fflate ~8KB, sudah ESM; fallback: JSZip. |
| `printf` shim bug format | Dukungan dibatasi `%d %u %x %s %c %%`; test di Fase 5. |

---

## 9. Checklist

- [ ] Fase 0 — `mode` dihapus, `args` ditambah, `docs/feats/app-model.md`
- [ ] Fase 1 — Unified host: terminal-default + argv + Terminal↔Gui
- [ ] Fase 2 — Raw canvas ABI (`wasm_canvas.cpp`) + flip Gui
- [ ] Fase 3 — Retained UI ABI (`wasm_ui.cpp`) + `ui_wait_event`
- [ ] Fase 4 — Input/timing (`input_*`, `delay`)
- [ ] Fase 5 — SDK DX: `printf` shim + header bersih
- [ ] Fase 6 — `.papp.zip` build + Forge auto-unpack
- [ ] Fase 7 — Examples + verifikasi G1–G9
- [ ] Fase 8 — Konvergensi JS/TSX + built-in (doc + no-regression)

---

## 10. Definition of Done (tiap fase)

Sesuai CLAUDE.md:
1. **STATE.md** — update baris status area `app`/`wasm` bila berubah + tanggal.
2. **docs/feats/app-model.md** — update kalau perilaku berubah.
3. **docs/decisions/** — tulis ADR untuk keputusan non-obvious (mis. "process-first &
   hapus AppMode", "ui_wait_event tanpa host callback").
4. **Checklist (§9)** — centang fase yang selesai.
5. **Commit** — conventional commit (`feat:`/`refactor:`/`docs:`).
6. **Success criteria fase** (§6) + **13/13 host test** hijau sebelum lanjut.

---

## 11. Backlog / keputusan tertunda

- **Backward-compat `mode`:** keputusan saat ini = **abaikan** field `mode` lama
  (parse-tolerant, perilaku jadi process-first). Tidak ada migrasi otomatis. (Konfirmasi
  user bila perlu strict-error.)
- Retained widget set awal minimal: title, text, button, row, col. Widget lain
  (list, input, checkbox) → backlog.
- Display server `fbcon` untuk ABI ini → backlog (sekarang `aether` saja).
- Konvergensi penuh JS/TSX ke ABI imports → Fase 8+ (sekarang cukup model + no-regression).
