# 76 — App & Service/Daemon Runtime Model (Headless Deployable Apps)

> Lengkapi model app supaya ada **dua kelas first-class**: **App** (foreground, punya UI
> surface, dilaunch user) dan **Service/daemon** (headless, autostart, long-lived, dikontrol
> remote). Ini inti visi "Akira" — board jadi **backend**: deploy `.kapp` `type:service` →
> jalan tanpa display → kontrol via CLI/Forge. Sekarang `AppType::Service` ada tapi cuma untuk
> **native built-in** (`installService(IService&)`); custom `.kapp` belum bisa jadi daemon.

- Status: 🔴 Not started
- Depends on: **38 (persistence — service tahan reboot)**, 47 (app platform map), 56/57/58
  (app runtime native/wasm/js), 51 (display server Aether), 74 (kontrol service via remote)
- Blocks: Forge CLI `deploy --service` (plan 78)

---

## 0. Keadaan sekarang

- `app_manifest.h`: `enum class AppType { App, Service }` + field `type`. **Sudah ada.**
- `app_registry.h`: `installService(IService& svc, id, ver)` — daftar **native** service
  (Flipper apptype=SERVICE), hidden dari launcher.
- `deauth/manifest.json` sudah menulis `runtime:"js"`, `display_server:"aether"`,
  `category`, tapi **belum** ada `autostart` dan belum ada jalur "`.kapp` → daemon headless".
- **Gap:** (1) custom/deployed app tak bisa jadi service; (2) belum ada `autostart`; (3)
  lifecycle service belum lepas dari foreground/ViewDispatcher; (4) belum ada kontrol
  start/stop/status yang seragam (lokal CLI + remote).

---

## 1. Goals — tiga kelas yang jelas

| Kelas | Surface? | Lifecycle | Contoh |
|---|---|---|---|
| **System service** (existing, `IService` di container) | — | boot→shutdown, dikelola ServiceManager | Clock, Gui, Input, Remote |
| **App** (existing) | ✅ pegang surface saat aktif | user-launch, single-foreground (AppHostManager) | Counter, Dolphin, WiFi UI |
| **Service/daemon** (BARU di plan ini) | ❌ **tak pernah minta surface** | enabled/running **lepas dari UI**, opsional `autostart`, tahan reboot | "telemetry uploader", "MQTT bridge", "BadUSB payload runner", deauth headless |

- [ ] Custom `.kapp` (JS/WASM/native) bisa dideklarasi `type:service` + `autostart:true`.
- [ ] Service jalan **headless** via Aether (tak akuisisi window/surface) di thread Nema sendiri.
- [ ] Lifecycle service **independen** dari foreground: tetap jalan saat user ke Home/launch app lain.
- [ ] Kontrol seragam: `start/stop/status/logs` — lokal (CliService) **dan** remote (PLP Cli, plan 74).
- [ ] **Persist + autostart**: service ter-deploy + flag autostart disimpan (plan 38) → hidup lagi setelah reboot. Ini makna "deploy app sebagai backend".
- [ ] **Resource & crash**: service kena akunting memori + crash-recovery (plan 64) — daemon mati ≠ device hang.

**Non-goal:** scheduling/cron (future), inter-service IPC kompleks, Forge CLI sendiri (→78).

---

## 2. Desain

### 2.1 Manifest
```jsonc
{ "id":"com.x.telemetry", "type":"service", "runtime":"js",
  "autostart": true,            // ← BARU: start saat boot
  "needs":["net.http"],         // capability-gated seperti app biasa
  "display_server": null }      // service tak punya surface
```
- Tambah field `autostart` (bool, default false) di `AppManifest`.
- Validasi: `type:service` ⇒ tak boleh akuisisi surface; runtime menolak panggilan UI.

### 2.2 Headless lewat Aether (plan 51)
- App = `IApp::run()` menggambar ke buffer + `present()`. **Service tak memanggil present()**;
  ia hanya jalan logikanya. Di Aether, service **tidak meminta Surface/window** → compositor
  tak pernah mengalokasikan glass untuknya. Inilah "tanpa display" yang bersih.
- Sediakan `ServiceContext` (subset `AppContext`: `runtime()`, `log()`, `storage`, timers,
  `shouldStop()`) **tanpa** `canvas()`/`present()`/input mailbox.

### 2.3 Lifecycle & manager
- Perluas/`ServiceHost` (analog `AppHost` tapi tanpa surface): spawn thread, jalankan
  `run(ServiceContext&)`, dukung stop/restart, lapor state.
- **Tidak** lewat `AppHostManager` single-foreground (itu untuk app). Service punya registry
  state sendiri: `Stopped → Starting → Running → Failed`.
- Boot: setelah `loadEmbeddedJsApps`/persisted-apps di-load, jalankan semua service
  `autostart:true`.

### 2.4 Kontrol seragam (lokal + remote)
- `CliService` commands: `svc list`, `svc start <id>`, `svc stop <id>`, `svc status <id>`,
  `svc logs <id>`. Karena RemoteService me-route Cli channel (plan 74, tier privileged),
  **Forge/Forge-CLI otomatis bisa mengontrol service** tanpa protokol baru.
- Event: `ServiceAppStarted/Stopped/Failed` ke EventBus (beda dari Service* milik ServiceManager).

### 2.5 Persistence (pakai plan 38)
- Deployed `.kapp` service disimpan di filesystem (plan 38); `autostart` flag di config/manifest.
- Reboot → loader meng-install dari flash → autostart service jalan lagi. **Loop deploy-backend lengkap.**

### 2.6 UI (opsional, kecil)
- Tambah baris "Services" di Settings→Developer (atau layar baru): list service + state +
  start/stop manual. Read-only minimum: tampil di Logs/Process monitor (plan 46).

## 3. Tasks
- [ ] `AppManifest`: tambah `autostart`; semantik `type:service` (no-surface) didokumentasi.
- [ ] `ServiceContext` + `ServiceHost` (headless, thread Nema, no surface).
- [ ] Loader: install service dari `.kapp` (JS/WASM/native) + autostart di boot.
- [ ] `CliService`: `svc list/start/stop/status/logs` (otomatis remote via plan 74).
- [ ] Event `ServiceApp*` + integrasi crash-recovery (plan 64) + akunting memori.
- [ ] Persist deployed service + autostart (pakai plan 38).
- [ ] Contoh: ubah `examples/deauth` jadi mode service headless (selain mode app) sebagai referensi.
- [ ] Build hijau 3 target + host tests untuk lifecycle service.

## 4. Acceptance criteria
- [ ] `.kapp` `type:service autostart:true` ter-deploy → jalan headless, **tak** mengambil layar.
- [ ] User ke Home / launch app lain → service tetap `Running` (lepas dari foreground).
- [ ] `svc stop/start/status` bekerja lokal **dan** dari remote (Forge) — gated auth (plan 74).
- [ ] Reboot → service autostart hidup lagi (persist plan 38).
- [ ] Service crash → recovery jalan, device tak hang; state `Failed` terlapor.
- [ ] Service memanggil API UI → ditolak/no-op (kontrak headless ditegakkan).
- [ ] Tak ada `#include` platform di `core/**`.
