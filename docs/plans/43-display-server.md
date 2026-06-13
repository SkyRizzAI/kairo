# 43 — Display Server (pluggable renderer + CLI substrate)

> Renderer berhenti jadi pipeline mati. Jadikan **backend yang bisa diganti
> runtime** (`IDisplayServer`): pixelate (canvas/UI sekarang), fbcon (console/TTY
> di panel), LVGL (board warna). CLI = **substrate yang selalu ada** (model
> AkiraOS / Linux getty); display server di-**launch dari CLI** di atas TTY
> (`display start pixelate`). Saat server stop/crash → **fallback ke fbcon/TTY**,
> device tak ikut mati. **Tidak ada autostart default di core** — board boleh
> inject service autostart sendiri; untuk demo, user yang menjalankan dari CLI.
>
> Hasil benchmark: **Flipper** render-nya hardwired (Canvas→u8g2), **AkiraOS**
> swap backend cuma compile-time + framebuffer destruktif tanpa compositor.
> **Tak satu pun bisa swap runtime** — di sinilah Palanu lebih maju.

- Status: ☐ Not started
- Milestone: M12 (Runtime Foundation — Display Server)
- Depends on: **Plan 42 (Capability & Resource Model)** — `available()`,
  `ResourceChanged`, CLI substrate, `resolve<T>()`; Plan 14 (UI Runtime),
  Plan 30 (Component runtime), Plan 27 (Input Abstraction), Plan 35 (KLP remote)
- Blocks: rich-UI evolution (LVGL apps), multi-modal display
- Catatan kode: `nema::` (rebrand `palanu` = Plan 41, belum jalan).

---

## Latar belakang

Pipeline sekarang **fixed**: `GuiService::renderOnce()` → `Canvas` →
`render(UiNode, Canvas)` (renderer.cpp:85) → `IDisplayDriver`. Tak ada interface
renderer, tak ada pemilihan, tak ada fallback. Tapi **tiga seam yang mahal sudah
ada** (Plan 42 + audit sebelumnya):

- **Semantic tree** `UiNode` (node.h:48-83) — terpisah dari rendering ✅
- **`IDisplayDriver`** (display.h:10-48) — sudah bersih & swappable ✅
- **`InputService`** standalone & thread-safe ✅

Jadi menjadikan renderer plug-in = **memformalkan satu interface**, bukan menulis
ulang. Yang kurang: pemilik loop (sekarang `GuiService`) belum tipis,
`ViewDispatcher` masih GUI-thread-only, dan belum ada konsep "server di-launch".

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Renderer | Fixed u8g2/st756x hardwired | Swappable **compile-time** (Kconfig); LVGL bypass FB | **Swappable runtime** via `IDisplayServer` |
| Compositor | Layered (status bar + app coexist) ✅ | Tidak ada; nimpa destruktif | Tiru layer Flipper (status bar persist) |
| CLI↔GUI | Paralel; tak bisa CLI-only | **Shell-first**, shell render ke TFT, `app start` ✅ | Tiru: CLI substrate, server di-launch dari CLI |
| Discovery | `furi_record_open` | `driver_registry` | `container().resolve<T>()` (sudah ada) |
| Crash | `direct_draw` acquire | tak ada isolasi | Fallback fbcon via liveness Plan 42 |

**Tak satu pun punya swap runtime** — Flipper mati, Akira compile-time. Goal kita
(pixelate↔LVGL via CLI di atas TTY) lebih maju dari dua-duanya.

---

## 1. Goal (acceptance tingkat-tinggi)

1. **`IDisplayServer`** — kontrak backend yang bisa diganti. Tiga kewajiban: consume
   `UiNode` tree, gambar ke `IDisplayDriver`, terima input dari `InputService`.
2. **`PixelateServer`** — renderer sekarang (`renderer.cpp`/`Canvas`/component-runtime)
   diekstrak jadi backend default. **Nol perubahan visual** dibanding sekarang.
3. **`FbconServer`** — backend teks: tampilkan **buffer CLI/console di panel**
   (analog Linux fbcon / AkiraOS `shell_display`). Sekaligus **fallback terakhir**.
4. **`LvglServer`** — backend build-time untuk board warna; reconcile `UiNode`→`lv_obj`.
   Bukti swappability (yang Akira pun cuma compile-time).
5. **`DisplayManager`** (service) — memegang `IDisplayDriver` + `ViewDispatcher` +
   drain input + DPM; memilih `IDisplayServer` aktif; fallback saat server stop/fault.
   `GuiService` menyusut jadi tipis / dilebur ke sini.
6. **CLI substrate + launch**: command `display`:
   ```
   display                       → status (server aktif, resolusi, fps)
   display list                  → backend tersedia (pixelate / fbcon / lvgl)
   display start <backend>       → mount server di atas TTY
   display stop                  → lepas → balik ke fbcon/TTY
   display switch <backend>      → ganti runtime tanpa reboot
   ```
7. **Kebijakan boot**: **core TANPA autostart default.** Saat cap display ada, boot
   berakhir di **fbcon/console** (CLI tampil di panel). User `display start pixelate`
   untuk UI. Board **boleh** inject `IService` autostart sendiri (post-boot panggil
   `displayManager.start("...")`) — core tetap bebas-policy. **Demo: tanpa autostart.**
8. **Fallback**: server `stop`/`display.fault` (event Plan 42) → `DisplayManager`
   jatuh ke fbcon. Device & CLI tetap hidup paralel lewat serial/KLP.
9. **Remote streaming tetap jalan**: `RemoteScreenTap` ada di layer `IDisplayDriver`,
   jadi **server apa pun** ter-stream ke Forge tanpa perubahan (properti gratis).
10. Teruji **host + WASM** (swap pixelate↔fbcon, fallback); build ESP32 OK.

**Non-goal (di luar scope):**

- **Window manager multi-window / surface per-app** — compositor cuma layer
  (status bar + 1 fullscreen), bukan window arbitrary. (Akira & Flipper pun tidak.)
- **`acquire<T>()`/refcount** display — satu server aktif pada satu waktu; tak ada
  kepemilikan contended. (lihat Plan 42 non-goal)
- **Sandbox capability per-app** ala Akira `cap_mask` — plan terpisah (App Permissions).
- **Protokol display jaringan baru** — `RemoteScreenTap`/KLP yang ada sudah cukup.

---

## 2. Arsitektur

```
                 ┌─────────────── CLI substrate (selalu ada) ───────────────┐
   serial/KLP ──▶│  CliService  ── command `display start|stop|switch ...`   │
                 └───────────────────────────┬──────────────────────────────┘
                                              │ launch / switch
                                              ▼
   InputService ─drain─▶  ┌──────────── DisplayManager (service) ───────────┐
   ViewDispatcher ──tree─▶│  • owns IDisplayDriver, loop, DPM, input route   │
                          │  • holds ACTIVE IDisplayServer                   │
                          │  • on stop/fault → fallback fbcon                 │
                          └───────────────┬──────────────────────────────────┘
                                          │ render(tree) / onInput(e)
              ┌───────────────────────────┼───────────────────────────┐
              ▼                            ▼                           ▼
       PixelateServer               FbconServer                  LvglServer
   (UiNode→Canvas→pixels,    (console buffer→panel,         (UiNode→lv_obj,
    = renderer sekarang)      fallback/TTY)                  build-time, warna)
              └───────────────────────────┴───────────────────────────┘
                                          ▼
                                   IDisplayDriver   ◀── (RemoteScreenTap decorator
                                                          → stream ke Forge, gratis)
```

### 2.1 Interface (`core/include/nema/ui/display_server.h`)

```cpp
struct IDisplayServer {
    virtual ~IDisplayServer() = default;
    virtual const char* name() const = 0;        // "pixelate" | "fbcon" | "lvgl"

    // Mount/unmount: ambil alih / lepas display + input.
    virtual bool mount(IDisplayDriver& display, InputService& input) = 0;
    virtual void unmount() = 0;

    // Per-frame (dipanggil DisplayManager): produksi pixel dari view aktif.
    virtual void renderFrame(ViewDispatcher& views) = 0;
    // Route input yang sudah di-drain manager (Action/Pointer).
    virtual void deliverInput(const InputEvent& e) = 0;
};
```

> `renderFrame(views)`: PixelateServer = `render(activeTree, canvas)` sekarang;
> FbconServer = abaikan tree app, gambar buffer console; LvglServer = reconcile
> tree→`lv_obj` lalu `lv_timer_handler()`. **Detail akses tree & sumber buffer
> console diselesaikan di eksekusi P1/P3** — interface ini titik tetapnya.

### 2.2 DisplayManager (`core/src/services/display_manager.cpp`)

- `IService`. Memegang loop (dipindah dari `GuiService`), `ViewDispatcher`, drain
  `InputService`, DPM. **`ViewDispatcher` dibuat thread-safe** di sini (P2).
- API runtime: `start(name)`, `stop()`, `switchTo(name)`, `active()`.
- Backend dari `container().resolve<...>()` / registry kecil by-name.
- Subscribe `events::ResourceChanged{display, fault}` → `stop()` aktif, mount fbcon.

### 2.3 Kebijakan boot (board-injected, core bebas-policy)

```cpp
// CORE: tak ada start otomatis. Boot → fbcon/console di panel.
// BOARD (opsional, TIDAK untuk demo): inject service autostart sendiri.
struct AutoDisplayService : IService {            // contoh, milik board
    void start() override { dm_->start("pixelate"); }
    ...
};
// Demo: user ketik `display start pixelate` di CLI.
```

### 2.4 Reuse Plan 42

`available(caps::Display)` untuk gate; `ResourceChanged` untuk attach/detach/fault;
CLI substrate (sudah lepas dari display di Plan 42 Fase 3); `resolve<T>()` discovery.

---

## 3. Fase pengerjaan

- [ ] **Fase 1 — Ekstrak `IDisplayServer` + `PixelateServer` (refactor, nol perubahan
      visual).** Bungkus pipeline sekarang jadi `PixelateServer`. `DisplayManager`
      memegangnya; boot tetap mount pixelate (parity). Host/WASM: output identik.
- [ ] **Fase 2 — `DisplayManager` memiliki loop + thread-safe `ViewDispatcher`.**
      Pindahkan loop/DPM/input-drain dari `GuiService`; `GuiService` jadi tipis/dilebur.
      Masih pixelate-only. Uji race redraw (lanjutan fix atomic 12 Jun).
- [ ] **Fase 3 — `FbconServer` + command `display` + boot CLI-first.** Console di panel.
      `display start|stop|switch|list`. Kebijakan: boot → fbcon; user launch pixelate.
      **Di sinilah perilaku berubah jadi CLI-first.** Uji headless + with-display.
- [ ] **Fase 4 — Fallback crash + hook autostart board.** `display.fault`/server-stop →
      `DisplayManager` mount fbcon (device tak mati). Sediakan pola `AutoDisplayService`
      board-injectable (didokumentasikan, **tidak diaktifkan**).
- [ ] **Fase 5 — `LvglServer` (build-time, board warna).** Reconcile `UiNode`→`lv_obj`;
      `display switch lvgl` runtime. Bukti swappability. ESP32 build-only.

**Build/uji:** host + WASM tiap fase (swap pixelate↔fbcon, fallback); ESP32 build-only
Fase 3 & 5.

---

## 4. Yang sengaja TIDAK dikerjakan (catatan masa depan)

- **Multi-window / surface per-app** — compositor hanya layer (status bar + 1 view).
  Window manager penuh = over-engineering MCU; Flipper & Akira pun tidak.
- **Autostart default di core** — selamanya policy board, bukan core.
- **`acquire<T>()`/refcount + display contended** — satu server aktif/saat.
- **Sandbox izin per-app (Akira `cap_mask`)** — plan App Permissions terpisah; ini
  satu-satunya area di mana Akira (keamanan) di depan Palanu, layak dikejar nanti.
- **Protokol display jaringan baru** — `RemoteScreenTap`/KLP cukup; berlaku lintas backend.
