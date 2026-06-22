# Plan 87 — App Capability, Radio HAL & Permission (Hybrid Raw Access + Crash Isolation)

> Status: **IN PROGRESS** (builder). Fase 0–4 ✅ — Fase 5 berikutnya.
> Prasyarat: Plan 84 (app runtime parity) ✅, Plan 85 (WASM bare-metal SDK) ✅,
> Plan 86 (process-first app model) ✅. Plan ini memberi app **akses hardware
> tingkat-rendah (radio raw)** dengan aman, lewat satu kontrak API yang
> di-generate ke tiga runtime.

---

## 0. Cara pakai dokumen ini (WAJIB dibaca AI Builder)

1. Sebelum mulai fase apa pun: baca `docs/STATE.md`, lalu **§1 Goals**, **§2
   Invariants**, dan **§3 Kondisi sekarang**. Jangan menyimpang dari goal.
2. Kerjakan **per fase, berurutan**. Tiap fase punya *success criteria* yang
   harus hijau sebelum lanjut.
3. **Jangan** menulis SDK per-bahasa dengan tangan. Semua binding app
   (C/WASM, JS, C++ built-in) **di-generate dari IDL** (`api/*.pidl` → `gen.ts`).
   Menambah API = 1 entri IDL + 1 impl host. Lihat §4.7.
4. **Jangan wifi-bias.** Wifi adalah *isi* pertama dari framework generik;
   arsitektur harus menerima BLE/SubGHz/NFC tanpa perubahan struktural.

---

## 1. Goals

### 1.1 Goal utama

Memberi app **akses penuh & aman ke hardware board** (mulai dari WiFi raw) melalui
**satu kontrak API generik** yang:
- di-generate seragam ke **tiga runtime** (C++ built-in · WASM · JS) — 1× kerja,
- di-**gate** otomatis oleh **capability + permission + lease**,
- menjaga **timing radio native** (tak bisa di-jitter app), dan
- **mengisolasi crash** app sehingga **tidak pernah** mengganggu kernel/firmware.

Pembuktian: satu **example app setara WiFi Marauder** ([ref](https://github.com/0xchocolate/flipperzero-wifi-marauder))
berjalan **on-device** sebagai app WASM/JS ter-sandbox.

### 1.2 Definisi sukses akhir (acceptance)

- [G1] **Kontrak tunggal → 3 runtime.** Satu fungsi radio di `api/*.pidl`
  otomatis muncul di header WASM, binding JS, interface C++ built-in, typings,
  docs, dan matriks parity. Tidak ada SDK yang ditulis tangan per bahasa.
- [G2] **Permission tiered.** Capability `sensitive` (mis. `net.wifi.monitor`)
  memunculkan **screen Allow/Deny**; capability `benign` (mis. cooked fetch)
  tidak. Grant **persisten** antar-launch; app bisa **query** status izinnya
  sendiri dan menangani penolakan (stop / tampilkan rationale).
- [G3] **Broker lease.** Resource eksklusif (radio) **single-owner**. App kedua
  yang minta mendapat `Busy{owner}`. Preempt hanya dengan **persetujuan user**.
- [G4] **Sistem menyesuaikan.** Saat app ambil mode WiFi eksklusif, koneksi
  sistem (NTP/HTTP) **otomatis suspend** (dengan banner) lalu **restore** saat
  lease dilepas. Lease sistem **non-preemptible** saat operasi kritis (OTA).
- [G5] **Hybrid raw.** Operasi berat/timing-critical (scan/deauth/beacon/sniff)
  jalan sebagai **primitive native** (loop di firmware Core 0); app dapat
  **event matang**. **Pintu raw** (inject + monitor ring buffer) tersedia untuk
  hal baru, dengan **backpressure** (drop, bukan stall).
- [G6] **Crash-safe.** App WASM/JS yang crash/hang **tidak** menjatuhkan kernel,
  dan **lease-nya otomatis dilepas** + network sistem **dipulihkan**. App
  offensive **wajib** WASM/JS (sandboxed).
- [G7] **Board-agnostik.** Wifi diimplementasi via HAL generik. Menambah
  BLE/SubGHz = tambah IDL + driver + capability, **tanpa** ubah broker/permission.
- [G8] **Example Marauder-equivalent** jalan on-device: izin → lease → suspend
  sistem → scan/deauth → restore.

### 1.3 Non-goals (JANGAN dikerjakan di plan ini)

- Implementasi BLE/SubGHz/NFC raw (cuma **disiapkan** slot-nya; isi = plan lanjut).
- App catalog / web-install 1-klik (Bangle-style) — backlog.
- Background widget / desktop surface (nyandar Plan 76) — backlog.
- MPU/memory-protection untuk app C++ native — di luar scope; ditutup oleh
  invariant "offensive = sandboxed" (§2).
- Signing/trust-chain untuk app — backlog.

---

## 2. Invariants (tidak boleh dilanggar)

| # | Invariant | Alasan / Goal |
|---|---|---|
| I1 | **Tak ada SDK ditulis tangan per-bahasa.** Semua binding dari IDL. | G1 |
| I2 | **Gating = annotation, bukan kode tangan.** `@capability`/`@tier` di IDL → emitter menyisipkan cek ke tiap binding. | G2 |
| I3 | **Loop timing radio selalu native (firmware Core 0).** Sandbox app (Core 1) tak pernah memegang loop real-time radio. | G5 |
| I4 | **Resource eksklusif = single-owner via Broker.** Tak ada akses radio langsung dari ABI tanpa lease. | G3 |
| I5 | **Offensive/untrusted app = WASM/JS only.** C++ built-in = first-party tepercaya (tak terisolasi dari hard-fault). | G6 |
| I6 | **Crash → lease auto-release + restore sistem.** Tidak boleh ada radio tertinggal di mode raw dengan network sistem mati. | G6, G4 |
| I7 | **Tipe API portabel** (scalar/string/bytes/`result`/record). Tak ada pointer/host-object di kontrak. | G1, G7 |
| I8 | **Vocab capability generik** (`net.wifi.*`, `bt.ble.*`, …). Tak ada nama chip/board di core. | G7 |

---

## 3. Kondisi sekarang (riset — file:line, untuk grounding)

**Fondasi yang SUDAH ada (plan ini memperluas, bukan bikin baru):**

- **IDL + 7 emitter**: `api/*.pidl` (9 file: `sys/input/media/bt/net/storage/profile/aether-ui/plp`)
  → `packages/idl/src/parser.ts` → `api/build/nema-api.json` (AST) →
  `packages/idl/src/gen.ts` fan-out:
  - `emit/wasm_c.ts` → `generated/sdk/nema.h` (header app WASM)
  - `emit/quickjs.ts` → `generated/host/nema_api_quickjs.gen.cpp` (binding JS)
  - `emit/dts.ts` → `generated/sdk/nema.d.ts` (typings)
  - `emit/host_cpp.ts` → `generated/host/nema_api.gen.h` (interface C++ built-in)
  - `emit/plp_ts.ts` → `packages/link/src/types-generated.ts`
  - `emit/parity.ts` → `docs/api/parity.md` (**matriks no-drift**)
  - `gen.ts --check` = deteksi file generated basi (CI-able).
- **Anotasi IDL**: `ast.ts` — `PidlInterface.annotations: { core, capability }`
  (capability **sudah ada** di level interface!), `PidlFunc.annotations: { blocking }`.
  `parser.ts:168` parse `@xxx("yyy")`; **anotasi tak dikenal diabaikan**
  (`parser.ts:204`, forward-compatible) → `@tier` aman ditambah.
- **Impl host tunggal**: `firmware/core/src/js/nema_host_impl.cpp`
  ("Hand-written implementation of HostApi") include `host/nema_api.gen.h`.
- **CapabilityRegistry**: `firmware/core/include/nema/system/capability_registry.h`
  — `add()/has()/list()` (statis, append-only) **+ `setState/stateOf/available()`
  dengan `ResourceState`** (live, owner-only). → **bibit broker sudah ada.**
- **Crash supervisor**: `firmware/core/src/app/app_host_manager.cpp` — subscribe
  `events::AppHostExited`, `crashMap_` per-app, `kMaxCrashes` circuit-breaker,
  **restart backoff** (langsung→5s→30s, baris 116-117).
- **Thread model**: `firmware/core/src/app/app_host.cpp:96` — tiap app di thread
  sendiri (Core 0); `threadEntry` (baris 201) bungkus `app.run()` dengan
  `try/catch(...)` → exitCode + post `AppHostExited`.
- **Sandbox**: WASM `firmware/core/src/wasm/wasm_engine.cpp` (trap→`M3Result`,
  tak crash host; **tapi tak ada fuel/interrupt** → loop tak-berujung spin).
  JS `firmware/core/src/js/js_engine.cpp:90` `setDeadlineMs` (**watchdog ADA**).
- **Config persist**: `firmware/core/include/nema/config/config_store.h` —
  `getString/setString/getInt/setInt/remove`. ⚠️ **NVS key ≤15 char** → permission
  tak bisa di-key `appId+cap`; pakai blob/file per-app.
- **Radio HAL**: `firmware/core/include/nema/hal/wifi.h` (`IWifi` cooked),
  `firmware/platforms/esp32/src/esp32_wifi_driver.cpp`,
  `firmware/platforms/esp32/src/esp32_ble.cpp`. Sim: `sim_wifi_driver.cpp` ("router").
- **Settings**: `app_list_screen.cpp`, `storage_settings_screen.cpp`,
  `settings_screen.cpp` (tempat menambah Apps→detail + relokasi storage).

**Gap yang plan ini tutup:** belum ada tier permission, belum ada broker lease,
belum ada radio raw ABI, belum ada watchdog WASM, storage app belum di app-detail.

---

## 4. Arsitektur target

### 4.1 Dua bidang (jangan saling ganggu)

- **Bidang 1 — App Runtime** (ambient · hot-path · tanpa gate): canvas, ui,
  input, timing, storage sandbox, device info, cooked fetch. Game/widget hidup
  100% di sini. **Tak pernah** menyentuh permission/lease → tak bisa di-bottleneck
  oleh tumpukan radio.
- **Bidang 2 — Capability** (cold-path · gated): hardware sensitif/eksklusif,
  diakses via pola seragam `acquire(cap) → lease`, di-parameter capability-string.

### 4.2 Tiga sumbu ortogonal (jangan dicampur)

| Sumbu | Pertanyaan | Mekanisme | Dicek kapan |
|---|---|---|---|
| **Capability** | board punya fitur ini? | `CapabilityRegistry.has()` | board-time |
| **Permission** | user izinkan app ini? | `PermissionService` (persist) | grant (sekali) |
| **Lease** | siapa pegang HW sekarang? | `ResourceBroker` | runtime, per-acquire |

Raw wifi butuh ketiganya. Cooked fetch: tak satupun (system-managed). Cek terjadi
**di `acquire` (cold), sekali**; operasi sesudahnya cuma validasi handle lease (murah).

### 4.3 Radio HAL generik + thick/raw (hybrid)

```
core/   IRadioWifi (abstrak): scan/deauth/beacon/handshake (thick)
                              monitor_open/read + inject (raw)
                              reportExclusivityGroups()
        IRadioBle  (slot disiapkan, isi = plan lanjut)
esp32/  Esp32WifiRadio : esp_wifi_set_promiscuous / _80211_tx / set_channel / AP
sim/    SimWifiRadio   : virtualize → feed "router" sim (dev di browser)
```

- **Thick** = firmware jalankan loop RX/TX **native** (Core 0), app dapat **event**.
- **Raw** = `inject(bytes)` passthrough + `monitor_read()` kuras **ring buffer**
  bersama (firmware drop saat penuh → radio tak pernah stall).
- **Core pinning** (sudah ada di thread model): controller+loop Core 0, app Core 1.

### 4.4 Broker lease + exclusivity groups

- `ResourceBroker` (service baru) **memiliki** driver radio tunggal; bangun di atas
  `CapabilityRegistry::setState/stateOf/available` yang sudah ada.
- API: `acquire(cap, prio) → Lease | Busy{owner,mode}`; `release(lease)`;
  `onRevoked(cb)`.
- **Exclusivity group dideklarasi driver** (`reportExclusivityGroups()`), bukan
  hardcode → board-agnostik. ESP32 single-PHY: `{wifi.managed, wifi.monitor,
  wifi.inject, wifi.ap}` satu grup; `{ble.*}` grup lain.
- **Auto-release**: subscribe `events::AppHostExited` → lepas semua lease app itu.

### 4.5 Permission model + tier

- Tier (annotation IDL): `ambient` (tak prompt) · `benign` (auto-grant, tak prompt)
  · `sensitive` (prompt Allow/Deny).
- `PermissionService`: simpan `(appId → {cap → granted|denied})` sebagai **blob**
  per-app (hindari batas NVS key 15-char — pakai file `/[data|system]/perm/<appId>.json`
  atau satu blob `setString("perm", <hash>, json)`).
- API app: `permission_status(cap) → granted|denied|not_asked`. App boleh stop /
  tampilkan rationale sendiri (ala iOS priming).
- Grant **persisten** sampai dicabut di Settings. Penggunaan radio aktif **selalu**
  tampilkan banner (transparansi), terlepas grant tersimpan.

> **Keputusan (planner): lama izin = persistent-until-revoked.** Grant disimpan,
> tak tanya ulang tiap launch. Per-app toggle "selalu tanya" **didukung model**
> tapi default off. Keamanan ekstra datang dari **banner aktif** + **lease
> per-sesi** (lease tak persisten — tiap sesi harus `acquire` ulang). Alternatif
> per-sesi penuh dicatat di Backlog bila perlu.

### 4.6 Crash isolation & resource cleanup

- **WASM/JS = sandboxed** → trap/exception tak sentuh kernel (sudah terbukti §3).
  **Offensive app wajib WASM/JS** (I5).
- **C++ built-in hard-fault TIDAK terisolasi** (no MPU) → hanya untuk first-party
  tepercaya. Didokumentasikan sebagai invariant, bukan ditutup teknis.
- **Watchdog WASM** (baru): tambah interrupt/deadline seperti JS `setDeadlineMs`,
  atau "no-progress" watchdog → app WASM hang bisa dibunuh supervisor.
- **Cleanup on exit/crash**: `AppHostExited` → Broker lepas lease → sistem restore.
  Reuse supervisor + backoff yang sudah ada.

### 4.7 IDL extension (kontrak → 3 runtime, otomatis ter-gate)

Tambah ke `FuncAnnotations`: `capability: string|null`, `tier: 'ambient'|'benign'|'sensitive'`.
Emitter menyisipkan, di tiap binding generated, prolog:
```
// (generated) — di host glue tiap runtime
if (!perm.check(app, "<cap>")) return ERR_PERMISSION;
if (!lease_valid(app, "<cap>")) return ERR_NO_LEASE;   // utk cap ber-lease
```
→ gating **identik** di WASM/JS/C++ dari **satu** deklarasi (I2). Parity matrix
menambah kolom `cap`/`tier`.

---

## 5. Kontrak teknis (IDL spec)

### 5.1 Anotasi baru
```
@capability("net.wifi.monitor")   // string capability
@tier(sensitive)                  // ambient | benign | sensitive
@lease                            // tandai butuh lease broker (eksklusif)
```

### 5.2 Interface radio wifi (`api/wifi.pidl` — baru)
```
@core
interface wifi {
  // ── lease (Bidang 2) ──
  @capability("net.wifi") @tier(benign)
  scan: func() -> result<scan-list, error>            // cooked, no lease

  @capability("net.wifi.monitor") @tier(sensitive) @lease
  monitor_open: func(channel: u8) -> result<unit, error>
  @capability("net.wifi.monitor") @tier(sensitive) @lease @blocking
  monitor_read: func(out: bytes, max: u32) -> result<u32, error>  // ring drain

  @capability("net.wifi.inject") @tier(sensitive) @lease
  inject: func(frame: bytes, channel: u8) -> result<unit, error>

  // ── thick primitives (loop native) ──
  @capability("net.wifi.inject") @tier(sensitive) @lease
  deauth_start: func(bssid: bytes, channel: u8) -> result<unit, error>
  deauth_stop:  func() -> result<unit, error>
  @capability("net.wifi.inject") @tier(sensitive) @lease
  beacon_spam_start: func(ssids: bytes) -> result<unit, error>

  // ── event matang (wait/poll, pola ui_wait_event) ──
  @blocking
  wait_event: func(out: bytes, timeout_ms: u32) -> result<u32, error>
}
```

### 5.3 Permission & lease API (`api/sys.pidl` — perluas)
```
@core
interface perm {
  status:  func(cap: string) -> u8          // 0=not_asked 1=granted 2=denied
  request: func(cap: string) -> u8          // memicu screen bila sensitive
}
@core
interface lease {
  acquire: func(cap: string) -> result<u32, lease-error>   // handle | Busy{owner}
  release: func(handle: u32) -> result<unit, error>
}
```

---

## 6. Fase per fase (goal · langkah · file · success criteria)

### Fase 0 — IDL annotation + gating codegen
- **Goal:** `@capability/@tier/@lease` diparse & emitter sisipkan gating ke 3 runtime.
- **Langkah:** extend `ast.ts` (`FuncAnnotations`), `parser.ts` (collect), tiap
  `emit/*.ts` sisipkan prolog cek; `parity.ts` tambah kolom cap/tier.
- **File:** `packages/idl/src/{ast,parser}.ts`, `emit/{wasm_c,quickjs,host_cpp,dts,parity}.ts`.
- **Success:** entri IDL beranotasi → generated WASM/JS/C++ semua punya cek;
  `gen.ts --check` hijau; parity matrix tampilkan kolom baru; unit test parser pass.

### Fase 1 — PermissionService + grant screen
- **Goal:** tier benign auto-grant; sensitive prompt; grant persisten; query API.
- **Langkah:** `PermissionService` (blob per-app, bukan NVS-key), `PermissionScreen`
  (Allow/Deny via hint footer), wire `perm.status/request` ke host impl.
- **File:** `firmware/core/src/services/permission_service.cpp` (+header),
  `firmware/core/src/screens/permission_screen.cpp`, `nema_host_impl.cpp`.
- **Success:** app sensitive → screen muncul; Deny → app dapat `denied` & handle;
  relaunch tak prompt ulang; benign tak prompt.

### Fase 2 — ResourceBroker + lease + auto-release
- **Goal:** resource eksklusif single-owner; revoke; auto-release on crash.
- **Langkah:** `ResourceBroker` di atas `CapabilityRegistry::setState`; exclusivity
  group dari driver; subscribe `AppHostExited`; wire `lease.acquire/release`.
- **File:** `firmware/core/src/services/resource_broker.cpp` (+header),
  `capability_registry` (reuse), `nema_host_impl.cpp`.
- **Success:** app A acquire → app B dapat `Busy{owner:A}`; app A exit/crash →
  lease lepas otomatis (uji via host test); preempt minta konfirmasi.

### Fase 3 — Koordinasi sistem (suspend/restore network)
- **Goal:** ambil mode eksklusif → suspend STA sistem + banner → restore saat lepas.
- **Langkah:** koneksi sistem pegang lease `net.wifi.managed`; broker suspend +
  emit event; `wifi_settings_screen` tampilkan banner; non-preemptible saat OTA.
- **File:** `resource_broker.cpp`, `esp32_wifi_driver.cpp`, `wifi_settings_screen.cpp`.
- **Success:** app monitor → NTP/HTTP berhenti + banner; lease lepas → reconnect
  otomatis; acquire saat OTA → ditolak `Busy{system}`.

### Fase 4 — Radio HAL generik + WiFi thick primitives
- **Goal:** `IRadioWifi`; scan/deauth/beacon/handshake jalan native Core 0 → event.
- **Langkah:** interface core + driver esp32 (`set_promiscuous/_80211_tx/channel`);
  loop native di Core 0; event matang via `wait_event`; sim virtualize.
- **File:** `firmware/core/include/nema/hal/radio_wifi.h`, `esp32_wifi_radio.cpp`,
  `sim_wifi_radio.cpp`, `api/wifi.pidl`.
- **Success:** app panggil `scan()` → list; `deauth_start` → frame keluar (uji sim
  router); UI tetap responsif (Core 1 tak terganggu).

### Fase 5 — WiFi raw escape (monitor ring + inject)
- **Goal:** pintu raw: `monitor_open/read` (ring + backpressure) + `inject`.
- **Langkah:** ring buffer firmware↔app; RX cb native append+drop; `inject` passthrough.
- **File:** `esp32_wifi_radio.cpp`, `wasm_*`/host glue (generated), `nema_host_impl.cpp`.
- **Success:** app baca frame mentah; app lambat → frame drop (radio tak stall,
  uji throughput); inject frame custom → terlihat di sniffer/sim.

### Fase 6 — Crash-safety hardening
- **Goal:** WASM watchdog; invariant offensive=sandboxed; verifikasi lease cleanup.
- **Langkah:** tambah interrupt/deadline ke `wasm_engine` (mirror JS); dokumen
  invariant I5/I6; host test: app crash mid-lease → lease lepas + sistem restore.
- **File:** `wasm_engine.cpp`, `docs/feats/app-capability-model.md`, tests.
- **Success:** app WASM `while(1)` → dibunuh supervisor; crash mid-monitor → STA
  reconnect + advertising/monitor berhenti; kernel hidup di semua kasus.

### Fase 7 — Settings: Apps→[app] detail + relokasi storage
- **Goal:** screen detail per-app: izin (toggle revoke) + storage (pakai/move/clear)
  + uninstall; storage app pindah dari Storage screen.
- **Langkah:** `app_detail_screen` baru; pindahkan logic per-app dari
  `storage_settings_screen` (jadi level-volume).
- **File:** `app_detail_screen.cpp`, `app_list_screen.cpp`, `storage_settings_screen.cpp`.
- **Success:** Settings→Apps→[app] tampil izin+storage; cabut izin → app berikutnya
  ditolak; Storage screen jadi volume-level.

### Fase 8 — Example app Marauder-equivalent + verifikasi goals
- **Goal:** app WASM/JS membuktikan G1–G8 end-to-end.
- **Langkah:** app contoh (scan/deauth/beacon UI) pakai primitive + raw; manifest
  deklarasi capability; build `.papp.zip`.
- **File:** `examples/wifi-marauder/` + manifest.
- **Success:** upload via Forge → izin → lease → suspend sistem → scan/deauth →
  restore; crash app tak ganggu device; jalan di sim (virtual) & hardware.

---

## 7. Test plan / parameter sukses keseluruhan

- **Unit (host)**: parser anotasi; PermissionService persist/query; Broker
  single-owner + auto-release; ring buffer backpressure.
- **Parity**: `gen.ts --check` hijau; `docs/api/parity.md` semua runtime tercakup.
- **Sim (browser)**: Marauder app jalan via "router" sim; suspend/restore network;
  crash app → kernel hidup.
- **Hardware (skyrizz-e32)**: scan/deauth nyata; banner; reconnect; WASM hang
  dibunuh; tak ada reboot kernel.
- **Build dual-target**: host + wasm + esp32 hijau.

---

## 8. Risiko & mitigasi

| Risiko | Mitigasi |
|---|---|
| Throughput frame mentah lewat sandbox → lag | Thick primitive default; raw via ring + backpressure (drop) |
| App C++ native hard-fault → kernel panic | Invariant I5: offensive = WASM/JS; C++ = first-party |
| WASM hang tak terbunuh | Fase 6 watchdog (interrupt/deadline mirror JS) |
| NVS key 15-char tak muat appId+cap | Permission = blob/file per-app |
| Lease bocor saat crash | Fase 2 auto-release via `AppHostExited` |
| Wifi-bias merembet ke struktur | Exclusivity group dari driver; capability generik (I8) |
| Suspend network ganggu OTA | Lease sistem non-preemptible saat operasi kritis (Fase 3) |

---

## 9. Checklist

- [x] Fase 0 — IDL `@capability/@tier/@lease` + gating codegen + parity
- [x] Fase 1 — PermissionService (tier, persist blob, query) + grant screen
- [x] Fase 2 — ResourceBroker (lease, exclusivity group, revoke, auto-release)
- [x] Fase 3 — Koordinasi sistem (managed lease, suspend/restore, OTA-guard)
- [x] Fase 4 — Radio HAL generik + WiFi thick primitives (native Core 0)
- [ ] Fase 5 — WiFi raw escape (monitor ring + inject + backpressure)
- [ ] Fase 6 — Crash-safety (WASM watchdog, invariant doc, lease cleanup verify)
- [ ] Fase 7 — Settings Apps→detail + relokasi storage
- [ ] Fase 8 — Example Marauder-equivalent + verifikasi G1–G8
- [ ] ADR 0007 — Hybrid raw radio access + lease/permission model

---

## 10. Definition of Done (tiap fase)

1. Build hijau (host + wasm + esp32 untuk fase yang menyentuh firmware).
2. `gen.ts --check` hijau bila menyentuh IDL.
3. Test (unit/sim) untuk fase tsb pass.
4. `docs/STATE.md` baris status diupdate bila area berubah.
5. `docs/feats/` dibuat/diupdate (app-capability-model, radio-hal).
6. Commit konvensional (`feat:`/`fix:`…) → changelog auto.

---

## 11. Backlog / keputusan tertunda

- BLE raw (`adv_raw`/`set_mac` spam) + koordinasi BLE-remote Forge.
- SubGHz / NFC / GPIO-SPI (modul add-on) — plug ke framework yang sama.
- App catalog + web-install 1-klik (Bangle-style).
- Background widget / desktop surface (nyandar Plan 76 service/daemon).
- Permission per-sesi penuh (bila tier "sangat berbahaya" perlu tanya tiap launch).
- App signing / trust-chain.
- MPU per-thread untuk app C++ native (bila kelak izinkan C++ pihak-ketiga).
