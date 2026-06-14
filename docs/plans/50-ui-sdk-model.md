# 50 — UI SDK Model (Per-Server) + Import/Binding

> UI **tidak universal**. Tiap display server mengekspos **UI SDK-nya sendiri**
> (namespace + font + widget + theme). App menarget satu server. Sekarang fokus
> **Aether** (`aether:ui`); server masa depan (Aurora/LVGL) ikut pola yang sama.

- Status: 🟧 Detail draft (belum diimplementasi)
- Depends on: 48 (System API non-UI), 49 (generator)
- Blocks: 52 (Aether UI SDK), 51 (negosiasi), 59 (manifest)

---

## Goals

- Menetapkan **pola per-server**: setiap display server punya UI SDK dengan
  namespace sendiri (sekarang `aether:ui`).
- Mendefinisikan **mekanisme import/binding** UI SDK ke 3 runtime:
  - **JS**: `import { View, Text, List } from "aether/ui"` (disediakan device, eksternal).
  - **WASM**: import interface `aether:ui` via SDK generated (crate Rust / header C).
  - **C built-in**: `#include <aether/ui.h>`, panggil host langsung.
- Field manifest `display_server` menentukan UI SDK mana yang di-wire saat launch.
- (Opsional) pustaka toolkit reusable (node-tree + flexbox, Ink-style) yang
  **dipakai Aether**; server lain bebas pakai/tidak.

## Keputusan

- **UI = per-server, bukan kontrak universal.** App = `headless` (System API saja,
  jalan di mana pun) atau `ui: aether` (terikat ke Aether).
- Lapisan **bersama hanya System API non-UI** (Plan 48); UI SDK terpisah per server.
- Akses UI = **import biasa per runtime** (di atas), di-resolve loader berdasarkan
  `display_server` di manifest.
- Fokus sekarang **Aether**; Aurora/LVGL menyusul lewat pola sama (belum diimplementasi).

---

## Latar belakang

### Kondisi `IDisplayServer` sekarang

`IDisplayServer` (`firmware/core/include/nema/ui/display_server.h:22-29`) baru
punya **dua kewajiban**: identitas (`name()`) dan menggambar satu frame view
aktif:

```cpp
struct IDisplayServer {
    virtual ~IDisplayServer() = default;
    virtual const char* name() const = 0;   // "aether" | "fbcon" | "lvgl"
    virtual void renderFrame(Canvas& c, ViewDispatcher& views,
                             const StatusBarData& status) = 0;
};
```

Dua backend konkret sudah ada dan terbukti bisa di-swap runtime:

- **`AetherServer`** (`ui/aether_server.h:13`, `src/ui/aether_server.cpp`) —
  renderer 1-bit default; mengomposit status bar + screen/modal ke `Canvas`
  lalu flush. Inilah kandidat UI SDK pertama (`aether:ui`).
- **`FbconServer`** (`ui/fbcon_server.h:13`) — konsol teks; **mengabaikan view
  tree app** dan menggambar info sistem. Fallback yang selalu ada → sengaja
  **tidak** punya UI SDK (server "headless surface").

Keduanya dimiliki `GuiService` dan ditunjuk `server_`; swap runtime dilakukan
lewat slot atomik `pendingServer_` yang di-apply di puncak loop GUI
(`src/services/gui_service.cpp:74-81`, `119-122`). `Runtime` meneruskannya
sebagai `switchDisplayServer()` / `displayServerName()` / `displayServerList()`
(`runtime.h:85-87`) untuk command CLI `display`.

### Pohon UI yang jadi dasar Aether SDK

Renderer Aether sudah memiliki seluruh tumpukan UI yang akan dipromosikan
menjadi isi `aether:ui`:

- **`UiNode`** retained-mode tree (`ui/node.h:48-83`) + `NodeArena` arena
  per-frame (`ui/widgets.h:11-28`).
- **Builder widget** (`ui/widgets.h`): `View/Text/Pressable/ScrollView`, lalu
  mid-level `Row/Col/Button/Header/Footer/ListRow/Toggle/Stepper/Select/Slider/
  TextField/Menu/Modal`.
- **Layout flexbox-subset** (`ui/layout.h:23`) dengan `TextMetrics` di-inject
  supaya murni logika & host-testable.
- **Renderer + Canvas** (`ui/renderer.h`, `ui/canvas.h`).

Semua ini hari ini di namespace `nema::ui` dan dipanggil **langsung** oleh screen
C++ in-tree. Yang **belum ada**: lapisan yang mengubahnya jadi *SDK yang
di-import* oleh app pihak-ketiga di 3 runtime, dan cara sebuah server
**mendeklarasikan** SDK itu agar loader bisa men-wire-nya.

### Yang kurang (jadi scope plan ini)

1. `IDisplayServer` belum mendeklarasikan UI SDK-nya (namespace, versi, capability
   minimal, tabel binding).
2. Belum ada kontrak `registerBindings()` yang dipakai loader WASM/JS untuk
   menyambungkan `aether:ui` ke import app.
3. Belum ada toolkit `nema::ui` yang dipisah sebagai pustaka opsional vs core.

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Akses UI dari app | `gui.h`/`view_port.h` di-link statis ke firmware; satu API global | LVGL `lv_*` di-link compile-time; tak ada SDK per-renderer | **UI SDK milik server**, di-import per namespace (`aether:ui`) — di-wire saat launch |
| Banyak renderer | Tidak — Canvas→u8g2 hardwired | Compile-time swap, tapi API app sama (`lv_*`) | Tiap server bawa SDK sendiri; app menyebut server target di manifest |
| Toolkit layout | Manual `canvas_draw_*`; tak ada flex | Flexbox LVGL = bagian renderer | **node-tree + flexbox = pustaka opsional** yang Aether pakai; server lain bebas |
| Binding non-C | Tidak ada (murni C) | Tidak ada | Generator (Plan 49) keluarkan binding C/WASM/JS dari IDL `aether:ui` |
| Headless | Tidak ada konsep | App selalu UI | App `headless` jalan tanpa server; bagian UI ditolak ala `cannot open display` |

Pelajaran inti: Flipper & Akira **mengunci satu API UI ke seluruh sistem**.
Palanu sengaja membalik — UI adalah properti *server*, bukan properti *kernel*,
sehingga menambah Aurora/LVGL kelak tidak menyentuh System API bersama.

---

## Desain teknis

### 1. Server mendeklarasikan UI SDK-nya

`IDisplayServer` diperluas (lihat juga Plan 51) dengan deskriptor UI SDK +
hook registrasi binding. Server **headless-surface** (fbcon) mengembalikan
`uiSdk() == nullptr` → tak ada UI yang bisa di-import.

```cpp
// firmware/core/include/nema/ui/ui_sdk.h  (baru)
namespace nema {

// Deskriptor satu UI SDK yang diekspos sebuah display server.
struct UiSdkDescriptor {
    const char* ns;            // "aether:ui"  (namespace import per-server)
    uint16_t    versionMajor;  // bump = breaking; manifest minta minor ≤
    uint16_t    versionMinor;
    const char* const* requiredCaps;  // {"display", "input.2d"}
    size_t             requiredCapCount;
};

// Host-side fungsi yang di-import app. Diregistrasi server ke import table
// runtime (WASM/JS). Signature flat = wasm3-friendly (Plan 48/49 convention).
struct IUiBindingHost {
    virtual ~IUiBindingHost() = default;
    // Daftarkan satu fungsi import: "aether:ui" / "view_begin" → fn.
    virtual void bind(const char* ns, const char* fn, void* hostFn) = 0;
};

} // namespace nema
```

```cpp
// display_server.h — tambahan pada IDisplayServer (lengkap di Plan 51)
struct IDisplayServer {
    virtual ~IDisplayServer() = default;
    virtual const char* name() const = 0;
    virtual void renderFrame(Canvas&, ViewDispatcher&, const StatusBarData&) = 0;

    // UI SDK yang diekspos server ini. nullptr = headless-surface (fbcon):
    // tak ada namespace UI untuk di-import.
    virtual const UiSdkDescriptor* uiSdk() const { return nullptr; }

    // Wire fungsi-fungsi UI SDK ke import table sebuah runtime app (WASM/JS).
    // Dipanggil loader saat app ber-display_server == name() di-launch.
    virtual void registerBindings(IUiBindingHost& host) {}
};
```

`AetherServer` mengisi keduanya:

```cpp
const UiSdkDescriptor* AetherServer::uiSdk() const {
    static const char* caps[] = { caps::Display, caps::Input2D };
    static const UiSdkDescriptor d{ "aether:ui", 1, 0, caps, 2 };
    return &d;
}

void AetherServer::registerBindings(IUiBindingHost& host) {
    // Tabel ini di-GENERATE dari IDL aether:ui (Plan 49), bukan ditulis tangan.
    host.bind("aether:ui", "view_begin",  (void*)&aether_abi::view_begin);
    host.bind("aether:ui", "text",        (void*)&aether_abi::text);
    host.bind("aether:ui", "list_begin",  (void*)&aether_abi::list_begin);
    // ... seluruh permukaan widget Aether (Plan 52).
}
```

Fungsi `aether_abi::*` adalah thin shim yang membangun `UiNode` di `NodeArena`
milik surface app (Plan 55) memakai toolkit `nema::ui` yang sudah ada — jadi
**implementasinya = pipeline render sekarang**, hanya dipanggil lewat ABI.

### 2. Wiring ke 3 runtime

Loader (AppHostManager / runtime tier, Plan 56–58) membaca `display_server` di
manifest. Bila `headless` → tak ada UI di-wire. Bila `aether` → ambil server
bernama `"aether"`, panggil `registerBindings()` pada import-host runtime
bersangkutan.

**C built-in** — `#include <aether/ui.h>` (header generated, Plan 49). App
compiled-in memanggil host langsung; tak ada import table, hanya linkage.

```c
#include <nema.h>          // System API non-UI bersama (Plan 48)
#include <aether/ui.h>     // UI SDK Aether (hanya untuk app display_server=aether)

void app_render(void) {
    AetherView root = aui_view_begin(AUI_COL);
    aui_text(root, "Hello", AUI_TITLE);
    aui_list(root, items, n);
    aui_view_end(root);
}
```

**WASM** — flat imports modul `aether:ui` (wasm3), di-resolve dari tabel yang
`registerBindings()` daftarkan. SDK crate Rust / header C di-generate.

```rust
// crate `aether-ui` (generated). Di balik layar = flat import:
//   (import "aether:ui" "text" (func ...))
use aether_ui::{View, Text, Flex};
use nema::storage;   // System API non-UI, interface "nema:storage/kv" (skema Plan 48)

#[no_mangle]
pub extern "C" fn render() {
    let root = View::col();
    root.child(Text::title("Hello"));
}
```

**JS** (QuickJS-ng) — module loader memetakan specifier `"aether/ui"` ke binding
native yang server registrasikan; `"nema"` ke System API non-UI.

```js
import { View, Text, List } from "aether/ui";   // hanya jika manifest aether
import { storage } from "nema";                  // selalu (non-UI bersama)

export function render() {
  return View.col([ Text.title("Hello"), List(items) ]);
}
```

Ketiganya berbagi **satu IDL `aether:ui`** dan **satu generator** (Plan 49):
parity by construction. Bedanya hanya amplop import per runtime.

### 3. Field manifest `display_server`

```jsonc
// manifest.json (skema lengkap Plan 59; format = JSON, BUKAN TOML — keputusan Plan 59)
{
  "id":             "com.example.clock",
  "runtime":        "js",        // c | wasm | js
  "display_server": "aether",    // "headless" | "aether"  (server masa depan: "aurora"…)
  "mode":           "ui",        // cli | ui | hybrid
  "needs":          ["storage", "net.wifi"]
}
```

- `display_server = "headless"` → loader tak men-wire UI apa pun; app jalan di
  board mana pun (System API non-UI saja).
- `display_server = "aether"` → loader **mewajibkan** server `aether` tersedia
  saat launch (negosiasi di Plan 51); bila tidak, bagian UI ditolak dengan
  pesan jelas ala `cannot open display` — bagian headless app boleh tetap jalan.
- Versi SDK dicek dari `UiSdkDescriptor.version*` vs manifest (opsional
  `display_server_version`); mismatch major = tolak.

### 4. Toolkit `nema::ui` (pustaka opsional, bukan core API)

Node-tree + flexbox + arena (`node.h` / `widgets.h` / `layout.h`) dipromosikan
jadi **pustaka toolkit** yang **Aether pakai** untuk mengimplementasi
`aether:ui`. Statusnya:

- **Bukan** bagian kontrak `IDisplayServer` — server lain (LVGL) **tidak wajib**
  memakainya (LVGL punya `lv_obj` + flexbox sendiri).
- **Bukan** System API bersama (Plan 48) — UI tetap per-server.
- Tetap di-link dalam core image (kecil, header-only-ish) sebagai utilitas; Aether
  ABI shim memanggilnya. Aurora kelak boleh me-reuse atau mengganti.

Artinya toolkit ini "library reuse", bukan "interface universal" — persis garis
keputusan Plan 47.

---

## Fase

- [ ] **Fase 1 — `UiSdkDescriptor` + perluasan `IDisplayServer`.** Tambah
      `ui_sdk.h`, `uiSdk()` + `registerBindings()` (default no-op) di
      `IDisplayServer`. `AetherServer.uiSdk()` mengembalikan `aether:ui` v1.0;
      `FbconServer.uiSdk()` tetap `nullptr`. Nol perubahan runtime (belum ada
      loader yang memanggil). Host build hijau.
- [ ] **Fase 2 — IDL `aether:ui` + generator.** Tulis IDL permukaan widget Aether;
      generator (Plan 49) keluarkan: tabel binding C++ (`aether_abi`), header C
      `<aether/ui.h>`, crate WASM, binding JS + `.d.ts`. `registerBindings()`
      memakai tabel generated.
- [ ] **Fase 3 — Wiring loader per runtime.** AppHostManager membaca
      `display_server`; untuk `aether` panggil `server.registerBindings()` ke
      import-host C/WASM/JS. `headless` lewati. Pesan tolak `cannot open display`
      bila server target absen (hook ke negosiasi Plan 51).
- [ ] **Fase 4 — Pisahkan toolkit `nema::ui`.** Dokumentasikan node-tree+flexbox
      sebagai pustaka opsional; pastikan Aether ABI shim = satu-satunya pemanggil
      via SDK (screen in-tree boleh tetap memakai langsung).

## File yang disentuh

- `firmware/core/include/nema/ui/ui_sdk.h` — **baru**: `UiSdkDescriptor`,
  `IUiBindingHost`.
- `firmware/core/include/nema/ui/display_server.h` — tambah `uiSdk()` +
  `registerBindings()` (selaras Plan 51).
- `firmware/core/include/nema/ui/aether_server.h` + `src/ui/aether_server.cpp` —
  implement `uiSdk()` (`aether:ui` v1.0) + `registerBindings()`.
- `firmware/core/include/nema/ui/fbcon_server.h` — `uiSdk()` tetap `nullptr`
  (eksplisit: headless-surface).
- `firmware/core/src/ui/aether_abi.{h,cpp}` — **baru, generated**: shim ABI
  `aether:ui` di atas toolkit `nema::ui`.
- Generator (Plan 49) + IDL `idl/aether-ui.*` — **baru**: SSOT permukaan `aether:ui`.
- Loader runtime (Plan 56–58, AppHostManager) — konsumsi `display_server` +
  `registerBindings()`.
- `docs/plans/59` — skema manifest `display_server`.

## Test

- **Host unit** — `AetherServer.uiSdk()` mengembalikan `aether:ui`, caps
  `{display, input.2d}`; `FbconServer.uiSdk() == nullptr`.
- **Generator/parity** — dari satu IDL `aether:ui`, jumlah & nama fungsi di
  binding C, WASM, JS identik (matriks parity, Plan 49).
- **Wiring** — fake `IUiBindingHost` merekam `bind()`; assert seluruh fungsi
  widget Aether terdaftar di namespace `aether:ui`, dan **tidak ada** saat
  app `headless`.
- **Tolak jelas** — launch app `display_server=aether` di host tanpa server
  aether → pesan `cannot open display`, bagian headless tetap jalan.
- **WASM** — app contoh meng-`import` `aether:ui`, render satu pohon, verifikasi
  `UiNode` terbentuk lewat shim.

## Risiko & mitigasi

- **ABI churn `aether:ui`** memecah app terpasang. → Versioning di
  `UiSdkDescriptor` (major/minor); generator emit matriks; manifest pin minor.
- **Godaan menjadikan `aether:ui` "API UI universal"** (regresi ke model lama).
  → Namespace bernama-server + `uiSdk()` per-server jadi pagar arsitektur; review
  menolak referensi `aether:ui` dari System API bersama (Plan 48).
- **Duplikasi toolkit** saat Aurora datang (dua node-tree). → Toolkit `nema::ui`
  sengaja "library, bukan kontrak": server bebas reuse; tidak dipaksakan.
- **Binding ditulis tangan menyimpang dari IDL.** → `registerBindings()` hanya
  memetakan tabel generated; CI gagal bila shim manual ditambah di luar generator.
- **Beban memori binding di MCU** (tabel import per app). → Tabel statik/const,
  shared antar instance; hanya di-wire untuk runtime non-C (C = linkage, 0 tabel).
