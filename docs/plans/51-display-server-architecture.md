# 51 — Display Server Architecture & Negotiation

> Memperluas `IDisplayServer` (Plan 43): tiap server mendeklarasikan UI SDK-nya +
> capability board; launch app dicocokkan dengan server target. Sekarang: Aether.

- Status: 🟧 Detail draft (belum diimplementasi)
- Depends on: 43 (display server), 50 (UI SDK model)
- Blocks: 52, 55

---

## Goals

- Mendefinisikan bagaimana display server **mendaftarkan UI SDK** + capability yang
  dibutuhkannya (display, input.2d, color, …).
- **Negosiasi launch**: app menyebut `display_server` target → board punya server
  itu? → UI jalan : ditolak dengan pesan jelas. **Bagian headless app tetap jalan**
  di shell meski server UI tak ada.
- Lifecycle: switch runtime (kalau >1 server di-build), fallback fbcon, crash
  isolation (lanjut Plan 43).

## Keputusan

- **App UI terikat satu server** — tak ada degrade/portabilitas lintas-server
  (model universal sudah dibuang). Cocok = jalan; tak cocok = tolak.
- Board tanpa server target → UI app ditolak jelas; logika headless-nya boleh jalan.
- fbcon = lantai/headless fallback yang selalu ada.
- Fokus Aether; multi-server (Aurora) ikut pola sama, belum diimplementasi.

---

## Latar belakang

### Apa yang sudah ada (Plan 43, done Fase 1–4)

Pluggable display server **sudah hidup** dan terbukti swap-runtime — plan ini
tidak membangun ulang, hanya **menambah lapisan negosiasi launch** di atasnya:

- **`IDisplayServer`** (`firmware/core/include/nema/ui/display_server.h:22-29`):
  `name()` + `renderFrame(Canvas&, ViewDispatcher&, StatusBarData&)`.
- **`AetherServer`** (`ui/aether_server.h:13`) = default UI 1-bit;
  **`FbconServer`** (`ui/fbcon_server.h:13`) = konsol teks + fallback.
- **Swap thread-safe** lewat slot atomik `pendingServer_`, di-apply di puncak
  loop GUI (`src/services/gui_service.cpp:74-81`, `119-122`).
- **Boot policy bebas-core**: backend awal dari config `display/boot`
  (default `fbcon`, CLI-first), bukan autostart core
  (`gui_service.cpp:42-46`).
- **Fallback fault**: subscribe `events::ResourceChanged` untuk `display`; bila
  resource tak `available` → `requestServer("fbcon")`
  (`gui_service.cpp:49-59`) — device tak pernah gelap.
- **API runtime**: `switchDisplayServer()` / `displayServerName()` /
  `displayServerList()` (`runtime.h:85-87`), dipakai command CLI `display`.
- **Capability model** (Plan 42): `CapabilityRegistry::has()` (statik) +
  `available()` (liveness) (`system/capability_registry.h:26-46`); katalog
  `caps::Display`, `caps::Input2D`, `caps::InputTouch`, dst
  (`system/capabilities.h`).

### Yang belum ada (scope plan ini)

1. Server belum **mendeklarasikan capability board minimal** yang dibutuhkan
   (Aether butuh `display` + input; LVGL butuh warna). `requestServer()` saat ini
   menerima nama tanpa cek kelayakan board.
2. Belum ada **negosiasi saat app launch**: mencocokkan `manifest.display_server`
   dengan server yang tersedia & layak, lalu menolak dengan pesan jelas atau
   menjalankan bagian headless.
3. Belum ada **registry server formal** — `GuiService` meng-hardcode
   `aether_`/`fbcon_` (`gui_service.cpp:76-77`, `87-92`). Untuk >1 server UI perlu
   tabel by-name.

`uiSdk()` + `registerBindings()` (Plan 50) adalah perluasan UI-SDK; plan ini
menambah **`requiredCaps()` + negosiasi launch + registry** di sisi arsitektur.

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Pemilihan renderer | Tetap (Canvas→u8g2), tak ada negosiasi | Compile-time (Kconfig); satu renderer per build | Runtime + **negosiasi per-launch**: app sebut server target, board jawab punya/tidak |
| App tanpa display | Tak ada konsep headless | App selalu UI | Manifest `headless` → jalan di shell tanpa server; UI ditolak ala `cannot open display` |
| Capability gating | `furi_hal_*` diasumsikan ada | Kconfig + `cap_mask` per-app (keamanan) | `requiredCaps()` server × `CapabilityRegistry` board (Plan 42) di-cek saat launch |
| Crash renderer | `direct_draw` acquire ad-hoc | Tak ada isolasi | Fallback fbcon via liveness `ResourceChanged` (sudah jalan, Plan 43) |
| Mismatch server | Tak mungkin (satu API) | Tak mungkin (compile-time) | **Tolak jelas** + bagian headless tetap jalan — properti baru |

Akira unggul satu hal: sandbox izin per-app (`cap_mask`). Itu **bukan** scope di
sini (plan App Permissions terpisah, sejalan catatan Plan 43). Yang Palanu
tambahkan dan keduanya tak punya: **negosiasi server↔board↔app saat launch**.

---

## Desain teknis

### 1. Server mendeklarasikan capability board minimal

`IDisplayServer` diperluas dengan `requiredCaps()` (melengkapi `uiSdk()` /
`registerBindings()` dari Plan 50). Negosiator memakai ini untuk menilai apakah
sebuah server **layak** di board tertentu.

```cpp
// firmware/core/include/nema/ui/display_server.h  (perluasan Plan 43+50+51)
struct CapList { const char* const* items; size_t count; };

struct IDisplayServer {
    virtual ~IDisplayServer() = default;
    virtual const char* name() const = 0;                              // Plan 43
    virtual void renderFrame(Canvas&, ViewDispatcher&,
                             const StatusBarData&) = 0;                 // Plan 43

    virtual const UiSdkDescriptor* uiSdk() const { return nullptr; }   // Plan 50
    virtual void registerBindings(IUiBindingHost&) {}                  // Plan 50

    // Capability board minimal agar server ini boleh di-mount/launch.
    // Aether: {display, input.2d}. fbcon: {display} (atau {} = selalu layak).
    // LVGL warna kelak: {display, color}.
    virtual CapList requiredCaps() const = 0;                          // Plan 51
};
```

```cpp
CapList AetherServer::requiredCaps() const {
    static const char* c[] = { caps::Display, caps::Input2D };
    return { c, 2 };
}
CapList FbconServer::requiredCaps() const {
    static const char* c[] = { caps::Display };   // konsol butuh panel saja
    return { c, 1 };
}
```

### 2. Registry server by-name (gantikan field hardcoded)

`GuiService` (atau `DisplayManager` di Plan 43 Fase 5) menyimpan tabel server
ber-nama, bukan dua pointer tetap. Server didaftarkan saat boot; board boleh
menambah backend-nya sendiri.

```cpp
class DisplayServerRegistry {
public:
    void add(IDisplayServer* s);                  // by name(), idempotent
    IDisplayServer* find(const char* name) const; // nullptr = tak di-build

    // Server yang LAYAK di board ini (requiredCaps semua available()).
    std::vector<IDisplayServer*> eligible(const CapabilityRegistry&) const;
    std::vector<const char*>     names() const;   // → displayServerList()
};
```

`requestServer()`/`switchDisplayServer()` sekarang menolak nama yang **ada tapi
tak layak** (board kurang cap) dengan alasan eksplisit, bukan diam-diam.

### 3. Negosiasi saat launch

Saat app di-launch, loader menjalankan negosiasi sebelum men-wire UI
(`registerBindings`, Plan 50). Hasilnya enum + alasan untuk pesan jelas.

```cpp
// firmware/core/include/nema/ui/display_negotiation.h  (baru)
enum class DisplayDecision : uint8_t {
    Headless,      // app tak minta UI → jalan di mana pun
    UiReady,       // server target ada + layak → wire UI SDK
    UiRejected,    // server diminta tak ada/tak layak → bagian UI ditolak
};

struct NegotiationResult {
    DisplayDecision decision;
    IDisplayServer* server;   // non-null hanya saat UiReady
    const char*     reason;   // untuk log + pesan "cannot open display"
};

NegotiationResult negotiateDisplay(const AppManifest& m,
                                   const DisplayServerRegistry& reg,
                                   const CapabilityRegistry& caps);
```

Logika (sengaja kecil & deterministik):

```
target = manifest.display_server          // "headless" | "aether" | …
if target == "headless":
    → Headless                            // selalu jalan
server = reg.find(target)
if server == null:
    → UiRejected  reason="display server '<target>' not built on this board"
if !caps.allAvailable(server.requiredCaps()):
    → UiRejected  reason="board lacks <cap> required by <target>"
if manifest.display_server_version.major != server.uiSdk()->versionMajor:
    → UiRejected  reason="aether:ui vX required, board has vY"
→ UiReady server
```

**Kontrak yang dikunci:**

- `Headless` / `UiReady` → app jalan; pada `UiReady` loader memanggil
  `server->registerBindings()` (Plan 50) sebelum start.
- `UiRejected` → **bagian UI ditolak**, tapi app **tidak** dibunuh otomatis:
  app `mode = hybrid`/`headless` melanjutkan jalur non-UI-nya; setiap panggilan
  UI mengembalikan error ala POSIX `cannot open display` (`reason` di atas).
  App `mode = ui` murni → launcher menampilkan `reason` & batal.
- Tidak ada **degrade lintas-server** (mis. memetakan `aether` ke `lvgl`): model
  universal sudah dibuang. Cocok atau tolak.

### 4. Lifecycle: switch, fallback, isolasi (lanjut Plan 43)

Mekanika Plan 43 dipertahankan, dibungkus negosiasi:

- **Switch runtime** (CLI `display switch <name>`): hanya untuk server yang
  `eligible()`. Mengganti backend *device*; **tidak** mengubah server *target
  app* yang sedang jalan (app `aether` tetap minta `aether`). Bila device
  di-switch ke server lain, surface app `aether` di-park sampai aether aktif lagi
  (kebijakan compositor, Plan 55).
- **Fallback fbcon**: tetap lewat `ResourceChanged{display, !available}` →
  `requestServer("fbcon")` (`gui_service.cpp:49-59`). fbcon `requiredCaps =
  {display}` → selalu lolos negosiasi sebagai lantai.
- **Crash isolation**: server fault → fallback fbcon; CLI substrate (serial/PLP)
  tak ikut mati. App yang ter-park menerima `UiRejected` transient sampai server
  pulih (tak crash app headless-nya).

### 5. Multi-server (Aurora/LVGL) — pola, belum diimplementasi

Menambah server kedua = (a) daftar ke `DisplayServerRegistry`, (b) isi
`uiSdk()` (`aurora:ui`) + `requiredCaps()`, (c) selesai. Negosiasi, registry,
fallback, dan CLI tak berubah. Plan ini memastikan jalur itu sudah ada walau
hanya Aether yang konkret sekarang.

---

## Fase

- [ ] **Fase 1 — `requiredCaps()` + `DisplayServerRegistry`.** Tambah
      `requiredCaps()` ke `IDisplayServer`; isi di Aether/fbcon. Ganti dua field
      hardcoded `GuiService` (`gui_service.cpp:76-77,87-92`) dengan registry
      by-name. `eligible()` menyaring via `CapabilityRegistry`. Perilaku runtime
      tak berubah (aether+fbcon saja). Host build hijau.
- [ ] **Fase 2 — `switchDisplayServer()` aware-kelayakan.** Tolak switch ke server
      tak-layak dengan alasan; `displayServerList()` = `registry.names()`. CLI
      `display list` menandai mana yang eligible.
- [ ] **Fase 3 — `negotiateDisplay()` + integrasi launch.** Implement negosiator
      murni (host-testable). Loader (Plan 56–58) memanggilnya sebelum
      `registerBindings()`. `headless` lewati; `UiReady` wire; `UiRejected`
      hasilkan `cannot open display` + biarkan bagian headless jalan.
- [ ] **Fase 4 — Park/restore surface saat switch + crash transient.** Surface app
      target di-park bila device di-switch/server fault; restore saat server
      target aktif lagi (selaras compositor Plan 55). Uji fallback tak membunuh
      app headless.

## File yang disentuh

- `firmware/core/include/nema/ui/display_server.h` — tambah `requiredCaps()`
  (selaras `uiSdk()`/`registerBindings()` Plan 50).
- `firmware/core/include/nema/ui/aether_server.h` + `fbcon_server.h`
  (+ `src/ui/*.cpp`) — implement `requiredCaps()`.
- `firmware/core/include/nema/ui/display_server_registry.h` + `.cpp` — **baru**:
  registry by-name + `eligible()`.
- `firmware/core/include/nema/ui/display_negotiation.h` + `.cpp` — **baru**:
  `negotiateDisplay()` + `NegotiationResult`.
- `firmware/core/src/services/gui_service.cpp` — pakai registry (ganti
  `aether_`/`fbcon_` hardcoded di `requestServer`/`serverNames`); switch
  aware-kelayakan.
- `firmware/core/src/runtime.cpp` (+ `runtime.h:85-87`) — `displayServerList()`
  dari registry; `switchDisplayServer()` kembalikan alasan tolak.
- Loader runtime (Plan 56–58, AppHostManager) — panggil `negotiateDisplay()`
  sebelum wiring UI.
- `docs/plans/55` (surface park/restore), `docs/plans/59` (manifest
  `display_server` + `mode`).

## Test

- **Host — `requiredCaps`** — Aether `{display, input.2d}`, fbcon `{display}`.
- **Host — `eligible()`** — board tanpa `input.2d` → aether tak eligible, fbcon
  eligible; board lengkap → keduanya.
- **Negosiasi (tabel)** —
  - manifest `headless` → `Headless` (board apa pun, termasuk tanpa display).
  - `aether` di board lengkap → `UiReady`, server non-null.
  - `aether` di board tanpa display/`input.2d` → `UiRejected`, reason menyebut cap.
  - `aurora` (tak di-build) → `UiRejected`, reason "not built".
  - major version mismatch → `UiRejected`.
- **Tolak tak membunuh headless** — app `hybrid` di-`UiRejected`: jalur non-UI
  tetap jalan; panggilan UI mengembalikan `cannot open display`.
- **Switch** — switch ke server tak-eligible ditolak beralasan; ke eligible OK.
- **Fallback** — `ResourceChanged{display,!available}` → fbcon mount; app headless
  tak crash (Plan 43 parity dipertahankan).
- **WASM** — paritas negosiasi host↔WASM untuk app contoh aether & headless.
- **ESP32** — build-only dev-board (registry + negosiator ter-compile).

## Risiko & mitigasi

- **Negosiasi tumpang-tindih dengan fallback Plan 43** (dua jalur memutuskan
  server). → Pembagian tegas: negosiasi memilih server **app target** saat
  launch; fallback memilih backend **device** saat fault. Keduanya lewat registry
  yang sama, tak saling menimpa.
- **Pesan tolak bocor detail/membingungkan user.** → `reason` terstruktur &
  pendek (`cannot open display: server 'aether' not built`); di-log via
  `rt.log().warn("DisplayNegotiation", …)` (bukan `printf`), tampil di launcher.
- **Registry menambah footprint untuk board single-server.** → Tabel kecil
  (vector pointer); fbcon+aether = dua entri; LVGL hanya bila di-build.
- **App `hybrid` separuh-jalan** (UI ditolak, headless lanjut) membingungkan.
  → Kontrak eksplisit di manifest `mode`: `ui` murni = batal saat reject; `hybrid`
  = lanjut headless; diuji.
- **Godaan auto-degrade ke server lain** saat target absen (regresi universal).
  → Dilarang by design: `UiRejected`, bukan substitusi. Review menolak fallback
  lintas-SDK.
- **Switch device saat app aether jalan** bisa membuat app "hilang". → Park/restore
  surface (Fase 4) + kebijakan compositor Plan 55; CLI `display` memperingatkan.
