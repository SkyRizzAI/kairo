# 55 — Surface & Window Model (Invoke-UI + WM Policy)

> Jahitan app↔compositor: app headless bisa "mengangkat window" via UI API
> (ala Linux app connect ke display server). Satu window/app sekarang, siap multi.

- Status: ✅ Implemented — ISurface interface defined; AppHost implements AppContext (surface + process); IWindowPolicy interface defined (`window_policy.h`); `SingleForegroundPolicy` concrete class implemented (`single_foreground_policy.{h,cpp}`): tracks foreground surface via `onSurfaceCreated`/`Destroyed`, `visibleSurfaces()` returns `{foreground}`, `focused()` returns the foreground surface
- Depends on: 51 (negosiasi), 52 (compositor), 54 (process)
- Blocks: runtimes (sisi UI)

---

## Goals

- API **init/create surface** (window) yang dipanggil app dari dalam shell.
- Lifecycle surface (create, draw/submit, input events, destroy).
- **WM policy** terpisah & swappable: v1 = single-foreground (kaya HP); future =
  multi-window (Android desktop mode) tanpa ubah kode app.

## Keputusan

- App **selalu** menggambar ke surface-nya, **tak pernah** asumsi memiliki layar.
- Jumlah-window = kebijakan compositor (Plan 52), bukan urusan app → "flexible
  nanti" hampir gratis sekarang.
- Konvensi resolution-independent (CLAUDE.md) menegakkan disiplin multi-window.

---

## Latar belakang

Plan 54 menjadikan app sebuah **proses** (argv/stdio/exit, default *headless*).
Plan 55 menambahkan satu hal: cara proses itu **mengangkat window** — menyambung ke
display server seperti app Linux `connect()` ke X/Wayland — lalu menggambar ke
**surface** yang diberikan server, bukan ke layar fisik.

**Yang sudah ada (dan kenapa hampir-jadi):**

- `AppHost` (`core/src/app/app_host.cpp`) sebenarnya **sudah** menjadi surface
  implisit: ia mengalokasikan dua buffer in-RAM (`drawBuf_`/`readyBuf_`,
  `app_host.cpp:66-71`), app menggambar lewat `Canvas` di atas `BufferDisplay`
  (`app_host.cpp:70-71`), `present()` mem-publish frame ke GUI thread dengan
  mutex + frame-seq (`app_host.cpp:166-178`), dan input dikirim balik via mailbox
  `nextInput()/waitInput()` (`app_host.cpp:180-191`). **Itu persis siklus
  draw/submit + input sebuah surface** — hanya saja tak ada API yang menamainya
  "surface", dan ia dipaksakan ke *setiap* app (termasuk yang seharusnya headless).
- Jembatan ke compositor: `AppHost` adalah `IScreen` yang di-`push` ke
  `ViewDispatcher` (`core/include/nema/ui/view_dispatcher.h:15`), `draw()`
  mem-blit frame app ke canvas nyata (`app_host.cpp:104-132`), `mode()` memilih
  `Normal` (status bar di atas) vs `Fullscreen` (`app_host.cpp:100-102`,
  `screen.h:12-16`).
- Rendering sudah pluggable: `IDisplayServer::renderFrame(Canvas, ViewDispatcher,
  StatusBarData)` (`core/include/nema/ui/display_server.h:28`), default
  `AetherServer` (`core/include/nema/ui/aether_server.h`). **Tapi compositor hari
  ini cuma layer status-bar + satu `IScreen` aktif** — Plan 43 secara eksplisit
  menyebut "multi-window / surface per-app" sebagai non-goal (`43-display-server.md`
  baris 94, 203).
- Kebijakan window = `AppHostManager` single-slot + pause/resume Plan 22
  (`core/include/nema/app/app_host_manager.h:18-44`). Itu **sudah** sebuah WM
  policy — hanya belum dipisah jadi komponen yang bisa diganti.

**Celah yang ditutup Plan 55:** (1) memberi nama dan API eksplisit pada surface
implisit `AppHost`, sehingga app **memilih** mengangkatnya (proses headless tak
bayar biaya buffer/thread-UI); (2) memisahkan **kebijakan window** dari mekanisme
surface, supaya v1 single-foreground bisa ditukar ke multi-window tanpa menyentuh
kode app. Arah ini sejalan Plan 51 (negosiasi server) dan Plan 52 (compositor
Aether "memiliki N surface/viewport").

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Akuisisi UI | `gui_add_view_port(gui, vp, layer)` — app daftar ViewPort | App render langsung ke framebuffer LVGL (nimpa) | **`createSurface(ctx, cfg)`** — app minta surface ke server, headless = tak minta |
| Geometri | App tahu 128×64; layer fullscreen/status | Fixed panel; app tahu ukuran | **Compositor tentukan ukuran**; app gambar dari `surface.width()/height()` (CLAUDE.md) |
| Compositing | `Gui` compositor: status bar + N ViewPort per layer ✅ | Tak ada compositor; destruktif | Tiru layer Flipper; surface = unit komposisi (Plan 52) |
| Kebijakan window | Satu app foreground (loader), ViewPort di-stack | Satu app foreground | **WM policy terpisah**: v1 single-foreground, swappable ke multi |
| Multi-window | Tidak (status bar + 1 app) | Tidak | **Arsitektur siap** (compositor komposit N surface); policy yang menahan, bukan API app |
| Input | `view_port_input_callback` per ViewPort | Event global | Surface punya mailbox (sudah ada: `app_host.cpp:180`), policy rute fokus |

Flipper `ViewPort` adalah cetak biru terdekat: app **mendaftarkan** permukaan ke
compositor `Gui` dan menggambar ke sana, compositor melapis status bar + app — app
tak pernah memiliki layar. Palanu menyamai itu lalu melampaui di dua titik: surface
**opsional** (proses bisa murni headless, Flipper selalu GUI) dan **WM policy
swappable** (Flipper hardwired single-foreground). AkiraOS = pelajaran negatif:
menggambar destruktif ke framebuffer tanpa compositor → tak bisa multi-window
selamanya; Palanu menjaga notion surface sejak awal justru untuk menghindari itu.

---

## Desain teknis

### 1. API surface (`core/include/nema/ui/surface.h`)

Surface = jendela tunggal yang dimiliki app: buffer gambar + siklus submit + aliran
input. App **tak pernah** menyentuh `Canvas` layar nyata atau `ViewDispatcher`
(persis kontrak `AppContext` sekarang, `app_context.h:11-13`) — tapi kini eksplisit
dan opsional.

```cpp
namespace nema {

struct SurfaceConfig {
    const char* title      = nullptr;  // untuk WM (judul window, ala Flipper app name)
    bool        fullscreen = false;     // true → minta seluruh layar (tanpa status bar)
    // width/height SENGAJA TIDAK ADA di sini → compositor yang menentukan geometri
    // (single-foreground = full; tiling = sebagian). App baca dari surface (§ bawah).
};

struct ISurface {
    virtual ~ISurface() = default;

    // — geometri (ditentukan compositor; app HARUS resolution-independent) —
    virtual uint16_t width()  const = 0;
    virtual uint16_t height() const = 0;

    // — draw / submit —  identik siklus AppHost canvas()+present() sekarang
    virtual Canvas& canvas() = 0;       // gambar di sini (Canvas atas buffer in-RAM)
    virtual void    submit() = 0;       // publish frame ke compositor (= present())

    // — input —  mailbox surface (sudah ada di AppHost: nextInput/waitInput)
    virtual bool nextInput(InputEvent& out) = 0;
    virtual bool waitInput(InputEvent& out, uint32_t timeoutMs) = 0;

    // — lifecycle —
    virtual void destroy() = 0;         // lepas surface (otomatis saat proses exit)
};

} // namespace nema
```

### 2. Cara app mengangkat surface (seam app↔server)

Surface **tidak** datang dari `ProcessContext` (Plan 54) — itu menjaga "proses
default headless". App memanggil API **UI SDK** (Aether, Plan 52) yang menegosiasi
ke display server (Plan 51) dan, jika cocok, mengembalikan surface:

```cpp
// Aether UI SDK (Plan 52) — "connect to display server", lalu raise window.
// Mengembalikan nullptr jika board tak punya server target (Plan 51): bagian
// headless app tetap jalan, hanya UI-nya ditolak dengan pesan jelas.
ISurface* surf = aether::createSurface(ctx /*ProcessContext&*/, { .fullscreen = true });
if (!surf) { ctx.err().writeStr("no display\n"); return; }   // tetap bisa headless

for (;;) {
    InputEvent e;
    if (surf->waitInput(e, 50)) { /* handle */ }
    auto& c = surf->canvas();
    /* draw pakai c.width()/c.height() — JANGAN hardcode 264×176 */
    surf->submit();
    if (ctx.shouldExit()) break;
}
surf->destroy();
```

Pemetaan ke runtime mengikuti Plan 50/52: **JS** `import { createSurface } from
"aether/ui"`, **WASM** import interface `aether:ui`, **C built-in** `#include
<aether/ui.h>`. Field manifest `display_server` (Plan 51/59) menentukan binding
mana yang di-wire saat launch.

### 3. Realisasi surface = `AppHost` yang dipecah (Plan 54 → 55)

Plan 54 mengekstrak peran *proses* dari `AppHost` jadi `ProcessHost`. Plan 55
mengekstrak peran *surface* yang tersisa jadi `Surface` konkret yang
mengimplementasi `ISurface` **dan** `IScreen`:

- **`ISurface` (sisi app):** `canvas()`/`submit()`/`nextInput()`/`waitInput()` =
  `drawBuf_`/`readyBuf_` + mutex + frame-seq + mailbox yang sudah teruji di
  `app_host.cpp:104-191`. Nyaris copy-as-is.
- **`IScreen` (sisi compositor):** `draw()` blit (`app_host.cpp:104-132`),
  `mode()` Normal/Fullscreen (`app_host.cpp:100-102`), `update()/onPointer()`
  feed mailbox (`app_host.cpp:83-98`). Inilah unit yang dilapis compositor.

Jadi `AppHost` lama = `ProcessHost` (54) + `Surface` (55). Surface dibuat **lazy**
saat `createSurface()` dipanggil; proses headless tak pernah mengalokasikan buffer.

### 4. WM policy terpisah & swappable (`core/include/nema/ui/window_manager.h`)

Kunci "flexible nanti": pisahkan **mekanisme** (surface punya buffer + ter-blit)
dari **kebijakan** (surface mana foreground, bagaimana ditata). `AppHostManager`
single-slot Plan 22 (`app_host_manager.h`) di-refactor jadi implementasi
`IWindowPolicy` pertama.

```cpp
struct IWindowPolicy {
    virtual ~IWindowPolicy() = default;
    // Surface baru diangkat → policy putuskan penempatan & fokus.
    virtual void onSurfaceCreated(Surface& s) = 0;
    virtual void onSurfaceDestroyed(Surface& s) = 0;
    // Compositor minta daftar surface yang harus dikomposit frame ini (urut z).
    virtual void visibleSurfaces(std::vector<Surface*>& out) = 0;
    // Rute input fisik → surface mana yang fokus.
    virtual Surface* focused() = 0;
};
```

- **v1 `SingleForegroundPolicy`** (perilaku HP sekarang, = `AppHostManager` Plan
  22): satu surface foreground; surface baru → yang lama di-**pause** (thread park,
  `app_host.cpp:184-191`), modal "Close & Open?" jika sudah ada yang paused.
  `visibleSurfaces` = `{status bar, foreground}`. **Nol perubahan perilaku** vs
  hari ini — hanya dibungkus interface.
- **future `TilingPolicy` / desktop mode**: `visibleSurfaces` kembalikan N surface;
  compositor (Plan 52) komposit semuanya + status bar dalam satu frame. **Kode app
  tak berubah** — app cuma tahu `surface.width()/height()` yang kini lebih kecil.

Penukaran policy = ganti implementasi `IWindowPolicy` yang dipegang compositor;
tak menyentuh `ISurface`, `ProcessContext`, atau app.

### 5. Compositing (sambungan ke Plan 52)

Hari ini `AetherServer::renderFrame` melapis status bar + `ViewDispatcher.active()`
(`display_server.h:28`, `aether_server.h:18`). Evolusi:

- **v1:** surface direalisasi sebagai `IScreen` yang di-`push` `ViewDispatcher`
  (jalur sekarang) → single-foreground "gratis" dari stack yang ada. Compositor tak
  berubah.
- **multi-window:** compositor minta `policy.visibleSurfaces()` lalu blit tiap
  surface ke region-nya (z-order) + status bar. Ini langkah `renderFrame` baru di
  sisi `IDisplayServer`/Aether (Plan 52), **bukan** perubahan API app — itulah
  alasan "flexible nanti hampir gratis".

`RemoteScreenTap` di layer `IDisplayDriver` (Plan 43) membuat semua ini ter-stream
ke Forge tanpa kerja tambahan — properti gratis lintas policy.

---

## Fase

- [ ] **Fase 1 — `ISurface` + ekstrak `Surface` dari `AppHost`.** Definisikan
      `ISurface`; pindahkan buffer/submit/mailbox/blit `AppHost` ke `Surface`
      (impl `ISurface` + `IScreen`). `ComponentApp` pakai surface eksplisit.
      Parity test: visual host+WASM identik sekarang.
- [ ] **Fase 2 — `createSurface()` lazy + path headless.** API `aether::
      createSurface(ctx, cfg)`; proses tanpa panggilan ini = murni headless (tak
      ada buffer/IScreen). Negosiasi Plan 51: board tanpa server → `nullptr` +
      pesan jelas, logika headless lanjut. Test: app headless tak alokasi surface;
      app UI angkat surface dari shell.
- [ ] **Fase 3 — `IWindowPolicy` + `SingleForegroundPolicy`.** Refactor
      `AppHostManager` (pause/resume + single-slot Plan 22) jadi policy pertama di
      belakang interface. Compositor tanya policy untuk visible+focus. Nol
      perubahan perilaku; test parity Plan 22.
- [ ] **Fase 4 — Hook geometri compositor-driven.** `SurfaceConfig` tanpa w/h;
      `surface.width()/height()` dari compositor; audit semua app draw
      resolution-independent (CLAUDE.md). Stub `TilingPolicy` (tak diaktifkan)
      sebagai bukti seam. Test: ubah geometri surface → app re-layout benar.

**Build/uji:** host + WASM tiap fase; ESP32 build-only Fase 1 & 3.

---

## File yang disentuh

- **Baru:** `core/include/nema/ui/surface.h` (`ISurface`, `SurfaceConfig`),
  `core/include/nema/ui/window_manager.h` (`IWindowPolicy`) +
  `core/src/ui/single_foreground_policy.cpp`, `core/src/ui/surface.cpp`
  (ekstrak dari `app_host.cpp`).
- **Diubah:** `core/src/app/app_host.cpp` → dipecah jadi `ProcessHost` (Plan 54) +
  `Surface` (di sini); `core/include/nema/app/app_host_manager.{h}` →
  `SingleForegroundPolicy`; `core/include/nema/app/component_app.{h,cpp}` (loop
  pakai `ISurface`); `aether_server`/`renderFrame` (jalur visibleSurfaces, Fase 4);
  `display_server.h` jika compositor perlu daftar surface.
- **UI SDK:** entry `aether::createSurface` (Plan 52) untuk C/WASM/JS; manifest
  `display_server` (Plan 51/59) menentukan binding.

---

## Test

- **Unit/parity (host):** Surface draw/submit/mailbox identik perilaku `AppHost`
  sekarang; `SingleForegroundPolicy` = pause/resume + modal Plan 22 tak berubah.
- **Headless:** proses tanpa `createSurface` tidak mengalokasikan buffer/IScreen;
  `ps` (Plan 46) tampilkan proses headless tanpa window.
- **Negosiasi (Plan 51):** board tanpa display server → `createSurface` =
  `nullptr` + pesan; bagian headless app tetap jalan.
- **Geometri:** surface diberi ukuran berbeda → app yang taat `width()/height()`
  re-layout benar (uji ComponentApp); regresi untuk app yang hardcode dimensi.
- **WASM:** angkat surface dari Forge, stream ke Forge via `RemoteScreenTap` tetap
  jalan; ESP32 build-only.

---

## Risiko & mitigasi

- **Memecah `AppHost` menyentuh jalur render terpanas.** → Fase 1 = ekstraksi
  mekanis (kode `app_host.cpp:104-191` nyaris copy); parity visual host+WASM jadi
  gate tiap fase. Pertahankan fast-path fullscreen `flushBuffer`
  (`app_host.cpp:118-122`) dan frame-seq atomic (fix 12 Jun) tanpa regresi.
- **App yang meng-hardcode 264×176 pecah saat multi-window.** → Fase 4 audit;
  `SurfaceConfig` sengaja tanpa w/h memaksa app baca dari surface; lint/review
  menegakkan konvensi CLAUDE.md (resolution-independent).
- **Over-engineering WM untuk MCU.** → Hanya `IWindowPolicy` + satu impl
  (single-foreground) yang dibangun; multi-window = stub seam, tak diaktifkan.
  Tak ada biaya runtime untuk window yang tak ada.
- **Negosiasi server gagal/tak ada display.** → `createSurface` boleh `nullptr`;
  kontrak "app tetap jalan headless" diuji (selaras Plan 51).
- **Kebocoran surface saat proses crash/exit.** → `destroy()` otomatis saat
  `ProcessHost` join (Plan 54); policy `onSurfaceDestroyed` lepas dari komposisi.

---

## Yang sengaja TIDAK dikerjakan (sekarang)

- **Multi-window/desktop mode aktif** — hanya seam + stub policy; aktivasi nanti.
- **Window decoration/dekorasi WM** (title bar draggable, resize handle) — geometri
  ditentukan compositor, bukan user, sampai desktop mode.
- **Surface >1 per proses** — satu window/app sekarang (Keputusan); arsitektur tak
  menghalangi, tapi tak diimplementasi.
- **Protokol surface jaringan baru** — `RemoteScreenTap`/PLP (Plan 43) cukup,
  berlaku lintas policy.
