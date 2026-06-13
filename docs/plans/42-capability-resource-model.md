# 42 — Capability & Resource Model

> Pisahkan dua hal yang sekarang digencet jadi satu string: **capability**
> (statis — "box ini *bisa* apa, selamanya") dan **resource liveness** (dinamis
> — "apa yang *hidup & bisa dipakai sekarang*"). Capability tetap string tapi
> didefinisikan di satu **katalog konstanta** (fleksibel & dinamis, tapi anti
> typo/duplikat). Liveness pakai satu **pola seragam** di atas EventBus yang
> sudah ada — berlaku untuk semua resource (display, camera, audio, net, bt,
> storage, remote-link) tanpa kode bespoke per-resource. Sekalian: lepaskan
> **CLI substrate dari ketergantungan display**, dan beri **JS API device yang
> bersih** (JS tak menyentuh registry mentah).

- Status: ✅ Implemented — host (9/9 ctest) + ESP32 dev-board & skyrizz-e32 build green.
  Deferred sub-items (low value / verification-gated): `registerDriver()` helper,
  WASM-vs-board layer unification, background-thread net/bt/remote liveness bridge.
- Milestone: M12 (Runtime Foundation — prasyarat Display Server)
- Depends on: EventBus + AsyncEventPoster (Plan 04), ServiceContainer (Plan 05),
  IService lifecycle (Plan 19.5/19.6), capability/hardware registry (Plan 08)
- Blocks: **Display Server** (plan menyusul) — fase ini fondasinya
- Catatan kode: file masih `nema::` / `nema/…`; rebrand ke `palanu` = Plan 41
  (belum jalan). Plan ini ditulis terhadap kode **sekarang** (`nema::`).

---

## Latar belakang

Hasil audit menyeluruh atas seluruh tree (di luar build/vendor):

- `CapabilityRegistry` (`core/src/system/capability_registry.cpp`) = `vector<string>`,
  append-only, `has()` linear, **boleh duplikat, tak bisa dicabut, tak ada event**.
- **23 capability string unik**, ditulis di **37 tempat** (14 duplikat) lintas
  platform + board.
- **Konsumen cuma 19 `has()`** di seluruh codebase; **14/19 saat boot**, hanya 1 di
  hot-path (`gui_service.cpp:56`, ikon wifi status bar). → sistem kecil, refactor
  **low-risk**.
- **Dua registry paralel tumpang-tindih**: `CapabilityRegistry` (flag string) dan
  `HardwareRegistry` (`{id, kind, detail}` by `DriverKind`). Driver wifi/ble/display
  mendaftar ke **dua-duanya manual** (double-bookkeeping).

### Lima masalah nyata (bukan kosmetik)

1. **Substrate di-gate ke display** — `esp32_platform.cpp:43`
   `if (!rt.capabilities().has("display")) return;` mematikan CLI/KLP/fs di board
   tanpa display. Ini salah-pakai capability check **dan** prasyarat display-server.
2. **Redundansi `has()` vs `resolve<T>()`** — `js_api.cpp:145`
   `has("profile") || resolve<ProfileService>()`: dua gerbang untuk fakta sama.
3. **Taksonomi string ad-hoc & rawan typo** — `wifi`+`networking`+`http` untuk satu
   driver; `bluetooth`+`bluetooth.ble`; tak ada tempat kanonik.
4. **Inkonsistensi produsen** — WASM mendaftar `display`/`input` di level *platform*,
   ESP32 menyerahkan ke *board*. Cap "conditional" (`remote.usb`, `storage`) = bentuk
   *derived capability* yang fragile/order-dependent.
5. **Satu sumbu, dua makna** — `has("display")` mencampur "punya panel" (statis) dan
   "panel hidup sekarang" (dinamis). Untuk yang disolder ini tak masalah; untuk
   **remote-screen lewat KLP** (yang attach/detach runtime) ini salah model.

### Infra liveness — sudah 80% ada (REUSE, jangan bangun ulang)

- **EventBus** (`event_bus.cpp`, sync, snapshot-safe saat dispatch) + **AsyncEventPoster**
  (`async_event_poster.cpp`, thread-safe dari bg-thread). Sudah punya
  `NetworkConnected/Disconnected`, `ServiceStarted/Stopped/Failed`, `Bt*`.
- **`resolve<T>()`** null-safe. Driver sudah punya state query: `ICamera::isOpen()`,
  `IWifiDriver::isConnected()`, `IBleAdapter::isConnected()`.
- **IService** punya `start/stop` + `ServiceState{…Failed}` + `ServiceManager::transition()`
  yang **sudah emit event**, plus `adoptService/dropService` runtime.
- **Kurang dibangun:** `LinkService::onDisconnect` (sekarang cuma `onReady`), dan satu
  event liveness seragam per-resource.

### Prinsip (hasil diskusi)

> **Capability statis** menjawab *"box ini bisa apa, ever"* → registry string.
> **Liveness dinamis** menjawab *"apa yang colok & hidup sekarang"* → state +
> event, **dimiliki oleh service pemilik resource**, dipancarkan lewat EventBus.
> **Jangan disentralisasi** (Linux pun tak punya registry dinamis pusat — DRM/V4L2/ALSA
> punya state sendiri, disatukan oleh *channel event* udev, bukan registry). Kairo
> sudah berbentuk itu: service per-resource + EventBus.

---

## 1. Goal (acceptance tingkat-tinggi)

1. **Katalog capability kanonik** `core/include/nema/system/capabilities.h`:
   `namespace caps { inline constexpr const char* Display = "display"; … }`. Semua
   produsen & konsumen rujuk konstanta ini — **nol string literal capability** tersebar.
2. **`CapabilityRegistry` dua-sumbu**, satu front-door `rt.capabilities()`:
   - `has(cap)` → **statis** (idempotent add, dedup, query set/O(1)-ish).
   - `available(cap)` → **dinamis** (apakah resource hidup sekarang).
   - `setState(cap, ResourceState)` → **hanya** dipanggil service pemilik resource;
     update cache + publish `events::ResourceChanged`.
3. **`ResourceState`** = `{ Absent, Available, Fault }` (di `types.h`). Default untuk
   cap statis yang tak melapor liveness = dianggap `Available` (backward-compatible:
   board solderan otomatis "available").
4. **Liveness untuk semua resource** lewat **satu pola seragam**: tiap service pemilik
   (`CameraService`, `AudioService`, `IWifiDriver`, BLE, VFS/storage, `LinkService`,
   display) memanggil `setState()` saat init sukses/gagal/teardown. Satu event
   (`ResourceChanged{resource, state}`), satu pola subscribe — N resource, bukan N kode.
5. **CLI = substrate tak bersyarat.** Wiring CLI/KLP/fs **keluar** dari gate
   `has("display")` di `esp32_platform.cpp:43`. Device headless (tanpa display) tetap
   fully usable lewat CLI. Hanya Canvas/GUI yang tetap bergantung display.
6. **JS API device bersih** — JS **tidak** menyentuh registry mentah. Surface kuratif:
   `nema.device.has(cap)` (statis) + `nema.device.available(cap)` (dinamis). Redundansi
   `has()||resolve()` di `js_api.cpp` dibersihkan.
7. **Registrasi driver sekali jalan** — helper `rt.registerDriver(d, kind, detail, {caps…})`
   menggantikan pasangan manual `hardware().add()` + `capabilities().add()`. Hapus
   double-bookkeeping; produsen ESP32 & WASM ikut aturan yang sama.
8. **Aturan konsumsi** terdokumentasi & diterapkan: `has()` hanya untuk **gate boot**;
   di dalam kode yang sudah jalan pakai `resolve<T>()`; untuk "hidup sekarang" pakai
   `available()` / subscribe `ResourceChanged`.
9. Teruji **host + WASM** (unit test registry dua-sumbu + event liveness); build ESP32 OK.

**Non-goal (eksplisit di luar scope):**

- **`acquire<T>()` dengan handle/refcount** (kepemilikan eksklusif resource yang
  contended) — pola query `available()` + `resolve<T>()` cukup dulu. Tambah saat ada
  kebutuhan nyata (mis. kamera diperebutkan dua app).
- **Display server / pluggable renderer** — plan terpisah; fase ini cuma fondasinya
  (CLI substrate + display sebagai resource dinamis).
- **Hotplug hardware solderan** — board solderan tetap statis-`Available`; liveness
  nyata cuma penting untuk resource yang memang colok-cabut/bisa-fault (remote-link,
  net, storage, camera-init).
- **Typed enum capability** — sengaja tetap string+katalog (fleksibel, JS-friendly,
  board bebas menambah cap baru tanpa ubah core).

---

## 2. Arsitektur

### 2.1 Katalog capability (`core/include/nema/system/capabilities.h`)

```cpp
namespace nema::caps {
// Display & input
inline constexpr const char* Display      = "display";
inline constexpr const char* Input        = "input";
inline constexpr const char* InputPrev    = "input.prev";
inline constexpr const char* InputNext    = "input.next";
inline constexpr const char* InputActivate= "input.activate";
inline constexpr const char* InputBack    = "input.back";
inline constexpr const char* InputAdjust  = "input.adjust";
inline constexpr const char* Input2D      = "input.2d";
inline constexpr const char* InputTouch   = "input.touch";
// Media
inline constexpr const char* Camera       = "camera";
inline constexpr const char* AudioInput   = "audio.input";
inline constexpr const char* AudioOutput  = "audio.output";
inline constexpr const char* Rgb          = "rgb";
// Sensors
inline constexpr const char* SensorsEnv   = "sensors.environment";
inline constexpr const char* SensorsLight = "sensors.light";
inline constexpr const char* SensorsMotion= "sensors.motion";
// Net & connectivity  (rapihkan: net.* menggantikan wifi/networking/http tersebar)
inline constexpr const char* NetWifi      = "net.wifi";
inline constexpr const char* NetHttp      = "net.http";
inline constexpr const char* BtBle        = "bt.ble";
// System
inline constexpr const char* Storage      = "storage";
inline constexpr const char* RemoteUsb    = "remote.usb";
inline constexpr const char* Profile      = "profile";
}  // namespace nema::caps
```

> Taksonomi dirapikan saat migrasi: `wifi`+`networking` → `net.wifi`, `http` →
> `net.http`, `bluetooth`/`bluetooth.ble` → `bt.ble`. **String = identitas wire/JS**
> (forward-compatible); **konstanta = pemakaian** (anti typo, terdokumentasi).

### 2.2 Registry dua-sumbu (`core/include/nema/system/capability_registry.h`)

```cpp
enum class ResourceState : uint8_t { Absent, Available, Fault };  // types.h

class CapabilityRegistry {
public:
    // Sumbu 1 — STATIS (inventaris): "box ini bisa apa, ever"
    void add(const char* cap);                 // idempotent (dedup)
    bool has(const char* cap) const;           // O(1)-ish (set)

    // Sumbu 2 — DINAMIS (liveness): "apa yang hidup sekarang"
    void setState(const char* cap, ResourceState s);  // owner-only; publish event
    ResourceState stateOf(const char* cap) const;     // Absent jika tak ada
    bool available(const char* cap) const;     // has(cap) && state==Available
                                               // (cap statis tanpa lapor = Available)

    const std::vector<std::string>& list() const;     // untuk `caps` CLI

private:
    std::unordered_set<std::string>            static_;   // sumbu 1
    std::unordered_map<std::string, ResourceState> live_; // sumbu 2
    EventBus* bus_ = nullptr;                  // di-set saat initCore
};
```

- `setState()` **satu-satunya** penulis sumbu dinamis, dan **satu-satunya** yang
  publish event → tak ada poller, tak ada race kepemilikan. Owner = service resource.
- `available()` default-Available untuk cap statis yang tak pernah `setState` →
  board solderan otomatis "hidup", **tak ada regresi** untuk 18 cap eksisting.

### 2.3 Event liveness seragam (`core/include/nema/event/event.h`)

```cpp
namespace events {
// … existing …
inline constexpr const char* ResourceChanged = "resource.changed";
}
// payload: {{"resource", "<cap>"}, {"state", "available"|"absent"|"fault"}}
```

> **Satu** event untuk semua resource — inilah yang bikin "lengkap semua resource"
> jadi murah: satu tipe event, satu pola subscribe, N resource. Konsumen yang peduli
> resource tertentu memfilter by `resource` field. Event lama (`NetworkConnected`,
> `BtConnected`, …) tetap; `setState` untuk net/bt cukup dipanggil di tempat yang
> **sudah** emit event itu (bridge, bukan ganti).

### 2.4 Pola owner (seragam, tipis) — contoh per-resource

```cpp
// CameraService
bool CameraService::open() {
    if (!cam_ || !cam_->open()) { rt_.capabilities().setState(caps::Camera, Fault); return false; }
    rt_.capabilities().setState(caps::Camera, Available); return true;
}
void CameraService::close() { rt_.capabilities().setState(caps::Camera, Absent); … }

// Storage (VFS mount di esp32_platform.cpp:88 — yang tadinya conditional add)
caps.add(caps::Storage);                       // statis: board bisa storage
caps.setState(caps::Storage, mountOk ? Available : Fault);

// LinkService (remote-screen attach/detach) — butuh onDisconnect BARU
link.onReady(    [&]{ caps.setState(caps::RemoteUsb, Available); });
link.onDisconnect([&]{ caps.setState(caps::RemoteUsb, Absent);   });  // ← dibangun

// Wifi/BLE — bridge di titik yang sudah emit NetworkConnected/BtConnected
caps.setState(caps::NetWifi, connected ? Available : Absent);
```

### 2.5 Registrasi driver sekali jalan (`core/include/nema/runtime.h`)

```cpp
// Ganti pasangan manual hardware().add()+capabilities().add() yang dobel:
template <class I, class T>
void Runtime::registerDriver(T* d, DriverKind kind, const char* detail,
                             std::initializer_list<const char*> caps);
// → container.registerAs<I>(d) + hardware().add({d->name(), kind, detail})
//   + untuk tiap cap: capabilities().add(cap). Satu sumber, tak ada lupa-sebelah.
```

`HardwareRegistry` & `CapabilityRegistry` **tetap dua struktur** (beda pertanyaan:
"chip apa + deskripsi" untuk `hwinfo`, vs "fitur apa" untuk gating) — yang disatukan
cuma **jalur registrasinya**.

### 2.6 CLI substrate tak bersyarat (`platforms/esp32/src/esp32_platform.cpp`)

```cpp
void Esp32Platform::postRegister(Runtime& rt) {
    // SUBSTRATE — selalu, tanpa syarat display:
    registerCoreCliCommands(cli_, rt);
    remote_.attachCli(cli_);
    // … KLP/transport wiring …

    // DISPLAY-ONLY — baru di sini cek display:
    if (rt.capabilities().has(caps::Display)) {
        // RemoteScreenTap (mirror layar) dst.
    }
}
```

### 2.7 JS device API bersih (`core/src/js/js_api.cpp`)

```js
nema.device.has("camera")        // statis: box punya kamera?
nema.device.available("camera")  // dinamis: kamera hidup sekarang?
```

- Backing: panggil `host->capabilities().has/available(cap)` — **JS tak pegang
  registry**, cuma API kuratif. Hapus `has("profile") || resolve<ProfileService>()`
  (pakai satu jalur). Objek `nema.http`/`nema.profile` di-gate via `available()`.

---

## 3. Fase pengerjaan

- [ ] **Fase 1 — Katalog + registry dua-sumbu (core, host-tested).**
      `capabilities.h` (konstanta), `ResourceState` di `types.h`, `CapabilityRegistry`
      (`add` idempotent, `has` via set, `setState/stateOf/available`, publish
      `ResourceChanged`). `event.h` tambah `ResourceChanged`. Unit test: dedup,
      static-vs-dynamic, default-Available, event terpancar. **Belum ubah produsen.**
- [ ] **Fase 2 — Migrasi konstanta + dedup produsen.** Semua `capabilities().add("…")`
      dan `has("…")` (37 + 19 lokasi) → konstanta `caps::`. Rapikan taksonomi net/bt.
      Tambah `Runtime::registerDriver(...)`; ubah driver wifi/ble/http/display agar
      daftar sekali jalan. WASM & ESP32 seragam (siapa deklarasi `display`/`input`).
- [ ] **Fase 3 — CLI substrate decouple.** Pindahkan wiring CLI/KLP/fs keluar dari
      gate `has(display)` di `esp32_platform.cpp:43`; hanya RemoteScreenTap/Canvas yang
      tetap di belakang `has(display)`. Verifikasi headless: build target tanpa display
      → CLI tetap jalan (uji di WASM + simulator dulu).
- [ ] **Fase 4 — Liveness owner wiring (semua resource, pola seragam).**
      `LinkService::onDisconnect` (BARU). Panggil `setState()` di: display (init/teardown
      + remote attach/detach), `CameraService` (open/close/fault), `AudioService`
      (in/out), wifi/ble (bridge di titik event eksisting), storage (mount). Tiap
      owner: 1–3 baris. Uji event `ResourceChanged` terpancar per resource (host/WASM
      fake drivers).
- [ ] **Fase 5 — Konsumen pakai sumbu yang benar.** Bersihkan `js_api.cpp`
      (`has`/`available`, buang redundansi). `nema.device.has/available`. Audit 19
      konsumen: gate boot tetap `has()`; status bar wifi (`gui_service.cpp:56`) →
      `available(caps::NetWifi)` + subscribe `ResourceChanged` (bukan poll). Settings
      screen: entry yang resource-nya bisa mati → pertimbangkan `available()` vs `has()`.
- [ ] **Fase 6 — Docs + build matrix.** Dokumentasikan taksonomi cap + aturan
      has/available/resolve di `CONTRIBUTING.md`/header. Build ESP32 (dev-board +
      skyrizz-e32) build-only; host + WASM full test hijau.

**Build/uji:** host + WASM tiap fase; ESP32 build-only di Fase 3 & akhir.

---

## 4. Yang sengaja TIDAK dikerjakan (catatan masa depan)

- **`acquire<T>()` + refcount/handle** — kepemilikan eksklusif resource contended
  (mis. kamera dipakai dua app). Pola `available()`+`resolve<T>()` cukup; tambah saat
  ada kebutuhan konkret.
- **Display server / pluggable renderer (`IRenderer`)** — plan terpisah. Fase ini
  menyiapkan fondasinya: CLI substrate berdiri sendiri + display jadi resource dinamis
  (event attach/detach + fallback saat fault sudah tersedia sebagai sinyal).
- **Hotplug HW solderan** — board solderan tetap statis-`Available`. Liveness nyata
  hanya untuk yang memang volatile (remote-link, net, storage, camera-init, audio).
- **Migrasi `HardwareRegistry`→ satu struktur** — tetap dua (beda pertanyaan); cuma
  jalur registrasi yang disatukan.
- **Typed enum capability** — tetap string+katalog demi fleksibilitas & JS/forward-compat.
