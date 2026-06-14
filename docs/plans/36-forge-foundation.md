# 36 — Palanu Forge (Web Client)

> **LAYER 3 dari 3** (Foundation (34) → Remote Layer (35) → **Forge**). Palanu
> Forge = web "all-in-one" ekosistem Palanu: **simulator** (WASM in-browser),
> **remote** (kontrol device fisik atau simulator), **flash/update**. Klien tunggal
> dengan **RemoteSession** yang transport-agnostic: simulator (virtual-cable ke
> WASM), device (Web Bluetooth / Web Serial) memakai UI & PLP yang sama. Karena
> simulator-WASM = device PLP penuh (Plan 35), **"remote bisa me-remote simulator"
> otomatis** — cukup pilih transport.

- Status: ✅ Fungsional penuh (client-side). Simulator WASM jalan di browser (multi-thread), `/simulator` & `/remote` berbagi satu instance, discovery list (Simulator/BLE/USB) ada. **Power lazy: halaman buka = device OFF (WASM TIDAK di-load); Boot → load+boot in-place; Restart → reload+autoboot; Shutdown → reload ke OFF (teardown WASM+workers).** **/flash: esptool-js (Web Serial) — pilih build dari registry, flash 3 part (0x0/0x8000/0x10000) + progress + serial console, full client-side.** **Firmware registry: tRPC `firmware.list`/`firmware.version` baca `static/firmware/manifest.json` (dihasilkan `firmware/tools/publish-firmware.sh`).** `bun run check` 0 error, `bun run build` hijau. Verifikasi flash fisik menunggu device terhubung. **(+Plan 37, aditif)** `RemoteSession.installApp(kapp)` + route **`/install`** (pilih/paste `.kapp` → push via PLP `Ext` ke device → install live di Apps). Jalur PLP sama untuk WASM virtual-cable & BLE/USB.
- Milestone: M9 (Board Profile & Ecosystem Foundation — Forge/Studio)
- Depends on: **35 (Nema Link Protocol & Remote Layer)**, **34 (BLE/USB foundation)**, 33 (Board Profile)
- Blocks: Palanu Studio (future)

---

## 0a. ARSITEKTUR FINAL — DIKUNCI (jangan menyimpang lagi)

> Aturan keras yang disepakati di awal. Semua perubahan WAJIB mengikuti ini.

1. **Simulator = WASM, SELALU.** Tidak ada simulator "native binary + server spawn"
   lagi. Firmware di-compile ke WebAssembly dan **jalan penuh di browser**
   (client-side); satu-satunya yang dari server = mengambil file `.wasm`. Forge
   adalah **full client-side** (SSR off).
2. **`packages/simulator` (React/Bun lama) DIHAPUS dari repo.** Bridge native di
   Forge (SimManager, tRPC `sim.*`, `sim.svelte.ts`) juga dihapus. Tidak dipakai.
3. **Satu instance simulator.** `/simulator` dan `/remote → Simulator` adalah
   **dua view dari SATU WASM yang sama** (`wasmSession()` singleton). Shutdown di
   satu = kena dua-duanya. (Native+WASM terpisah = bug yang sudah diperbaiki.)
4. **Remote = discovery list** (`/remote`): pilih target — **Simulator (WASM)**,
   **Bluetooth/BLE**, **USB**. Kalau pilih Simulator dan simulatornya hidup,
   remote **benar-benar terhubung ke simulator yang itu** (instance yang sama).
5. **Bridge remote↔simulator = interface USB/BLE VIRTUAL.** Simulator-WASM punya
   HAL yang sama dengan device asli (`IUsbCdc`, `IBleAdapter` — Plan 34). Di WASM,
   interface itu di-back oleh implementasi **virtual (in-process)**. Remote
   menyambung lewat interface itu **persis seperti ke hardware asli** — Forge tak
   bisa membedakan simulator dari device fisik selain transport-nya virtual.
   Protokol di atasnya tetap **PLP yang sama** untuk semua (BLE/USB/virtual).

```
                 Forge (full client-side, SSR off)
        /simulator  ────┐          /remote (discovery list)
        (view)          │          ├─ Simulator (WASM)  ─┐
                        ▼          ├─ Bluetooth (BLE)     │ pilih transport
                  satu wasmSession()                      │
                        │  PLP                            ▼
   ┌────────────────────┴───────────────────────────────────────┐
   │ WASM firmware (multi-thread, di browser)                    │
   │  interface VIRTUAL:  USB-CDC · BLE · (cable)  ← remote nyambung ke sini │
   └────────────────────────────────────────────────────────────┘
   device fisik: interface sama, tapi USB/BLE asli (Plan 34) — PLP identik
```

6b. **`/simulator` = UI KAYA seperti versi lama, BEDANYA cuma engine = WASM.**
   Layout 3 kolom WAJIB dipertahankan: (kiri) device screen + tema (eink/phosphor/
   amber) + HardwareButtons (d-pad/OK/Cancel); (tengah) Settings tabs (WiFi/
   Display/System); (kanan) Logs / Events / Services. Datanya lewat PLP:
   screen/input/log/**event**/**service** channel + perintah sim (inject event,
   wifi router) via channel kontrol. JANGAN sederhanakan jadi cuma layar+tombol.
7. **Serverless / client-side penuh.** Tidak ada proses server yang menjalankan
   firmware. Forge = static + client JS + WASM. **API (tRPC) HANYA** untuk hal
   server sejati: mengambil file firmware (`.bin` untuk OTA / `.wasm`), registry
   versi, dan sejenisnya — **bukan** untuk menjalankan simulator. Bisa deploy ke
   static host (cukup set header COOP/COEP + serve `.wasm` dengan COEP/CORP).

> Status implementasi 0a: poin 1,3,4,6 ✅ jalan. Poin 5 (virtual USB/BLE interface)
> saat ini diwujudkan oleh `WasmCableTransport` (PLP in-process) yang berperan
> sebagai interface virtual; penajaman agar benar-benar memetakan `IUsbCdc`/
> `IBleAdapter` virtual = lanjutan. Poin 2 (hapus `packages/simulator` + bridge
> native) = dikerjakan.

---

## 0. Latar belakang & posisi

Layer 2 (Plan 35) memberi **device-side remote** (PLP + RemoteService + transport
BLE/USB/virtual-cable + WASM firmware). Forge = **klien web**-nya: merender layar,
mengirim input, menampilkan log, mengontrol power, flashing, update.

Forge berdiri di atas fondasi yang **sudah dibangun** (Fase 1–3): scaffold
SvelteKit + Tailwind + shadcn-svelte + tRPC, dan simulator yang dimigrasikan dari
`packages/simulator` (React/Bun) ke Forge dengan **bridge native interim** (server
spawn `palanu-sim`, telemetry via tRPC SSE). PLP codec TS juga sudah ada & teruji
(`src/lib/plp/`). Plan ini melanjutkan ke **WASM + remote + flash**.

### Keputusan stack (terkunci)
SvelteKit (routing + API `+server`/tRPC) · Tailwind v4 · shadcn-svelte · tRPC v11
(command/query type-safe) · telemetry hot-path: SSE (interim) → PLP (final).

---

## 1. Goal

1. **Simulator → WASM in-browser** (gantikan bridge native): host `nema.wasm`
   (Plan 35) di **Web Worker**; `VirtualCableTransport` (postMessage) bicara PLP.
2. **RemoteSession (TS)** — transport-agnostic: `{ screen, logs, info, sendAction,
   sendPointer, power, ext }`. Dipakai `/simulator` & `/remote` IDENTIK.
3. **Transports (TS)** sejajar: `VirtualCableTransport` (WASM Worker),
   `BleTransport` (Web Bluetooth), `SerialTransport` (Web Serial).
4. **LinkHub** — fan-out satu device/WASM ke banyak view (mis. `/simulator` &
   `/remote` menonton instance WASM yang sama → "remote me-remote simulator").
5. **/remote** — pilih transport (Simulator-WASM / BLE / USB-Serial) → pair/handshake
   → stream layar + kontrol.
6. **/flash** — Web Serial flashing (esptool-js) + serial console.
7. **OTA registry** — tRPC `firmware.*` (list/serve firmware untuk OTA pull).

---

## 2. Arsitektur (web client)

```
        Forge RemoteSession (TS) — satu UI, satu PLP codec
   screen <canvas> · on-screen controls · logs · power · ext
                          │
                       LinkHub (fan-out: N view ⇄ 1 device)
                          │  ILinkTransport (TS)
        ┌─────────────────┼──────────────────┬────────────────┐
   VirtualCableTransport  BleTransport     SerialTransport   (WsTransport)
   (Web Worker postMsg)   (Web Bluetooth)  (Web Serial/USB)  (WiFi, opsional)
        │                   │                  │
   nema.wasm           device BLE         device USB-CDC
   (firmware, Plan 35)  (Plan 34/35)       (Plan 34/35)
```

Semua transport mengimplement `ILinkTransport` (`src/lib/plp/transport.ts`, sudah
ada). PLP codec (`src/lib/plp/codec.ts`, sudah ada & teruji) sama untuk semua.

```
packages/forge/src/
├─ lib/
│  ├─ plp/                      # ✅ codec.ts, transport.ts, codec.test.ts (DONE)
│  ├─ transport/
│  │  ├─ VirtualCableTransport.ts   # spawn Web Worker(nema.wasm), postMessage
│  │  ├─ BleTransport.ts            # Web Bluetooth GATT (PLP char notify/write)
│  │  └─ SerialTransport.ts         # Web Serial (PLP + flash/console)
│  ├─ RemoteSession.ts          # transport + PLP → screen$/logs$/info/send/power
│  ├─ LinkHub.ts                # fan-out 1 device → N session view
│  ├─ flash/esptool.ts          # esptool-js (Web Serial)
│  ├─ sim.svelte.ts             # ✅ store (DONE, native bridge) → migrasi ke RemoteSession
│  ├─ server/sim-manager.ts     # ✅ bridge native interim (DONE) → dihapus saat WASM cutover
│  ├─ trpc/                     # ✅ router/context/client (DONE) + firmware.* (OTA)
│  └─ components/               # ✅ DeviceScreen, HardwareButtons, panels (DONE)
├─ routes/
│  ├─ simulator/+page.svelte    # ✅ DONE (native) → cutover ke VirtualCableTransport
│  ├─ remote/+page.svelte       # RemoteSession(BLE | VirtualCable) — pair→stream
│  ├─ flash/+page.svelte        # Web Serial flash + console
│  └─ api/                      # tRPC: firmware.* (OTA registry)
├─ worker/palanu-worker.ts       # host WASM, jembatani postMessage ↔ wasm I/O
└─ static/nema.wasm            # output Emscripten (Plan 35 build:wasm)
```

### RemoteSession (inti — transport-agnostic)
```ts
class RemoteSession {
  constructor(t: ILinkTransport) { /* PLP codec + handshake on connect */ }
  readonly screen: Readable<ImageBitmap>;   // SCREEN channel
  readonly logs:   Readable<LogEntry>;        // LOG channel
  info(): DeviceInfo;                          // SYSTEM (model/fw/board profile)
  sendAction(a): void; sendPointer(p): void;   // INPUT
  power(op: 'restart'|'sleep'|'shutdown'): void;
  ext?: { timeScale(n): void; step(): void };  // WASM-only (EXT channel)
}
```
`/simulator` & `/remote` **import komponen yang sama**; beda cuma konstruksi
transport. Itu buah dari "simulator = remote session".

---

## 3. "Remote me-remote simulator" — cara kerjanya

1. WASM (Plan 35) jalan di Worker, expose PLP via `VirtualCableTransport`.
2. `LinkHub` membungkus satu transport dan mem-fan-out ke banyak `RemoteSession`.
3. `/simulator` attach RemoteSession#1 (lokal). `/remote` (tab/halaman lain) pilih
   transport **"Simulator (WASM)"** → attach RemoteSession#2 ke hub yang sama.
4. Dua-duanya lihat layar & kirim input ke WASM yang sama, real-time. Persis seperti
   me-remote device fisik — cuma transport-nya virtual-cable.
   *(Cross-device — HP me-remote sim di laptop — = relay PLP lewat server WS; future.)*

---

## 4. Status saat ini (Fase 1–3 SELESAI)
- ✅ Scaffold SvelteKit 2 + Svelte 5 + TS + Tailwind v4 + adapter-node
- ✅ shadcn-svelte (Nova) + komponen (button/card/tabs/scroll-area/badge/…)
- ✅ tRPC v11: `sim.*` (status/boot/shutdown/restart/build/setResolution/command/telemetry SSE) + `firmware.*` (kosong)
- ✅ Bridge native interim: `SimManager` spawn `palanu-sim` + telemetry SSE + snapshot replay
- ✅ App-shell (nav Simulator/Remote/Flash, tema dark) + redirect
- ✅ Simulator paritas penuh: DeviceScreen (3 tema) + HardwareButtons (+keyboard) + WiFi router + Display presets + System (inject + **Build dari web**) + Logs/Events/Services
- ✅ PLP codec TS + test (`src/lib/plp/`, 7/7) — siap dipakai RemoteSession
- ✅ Root scripts `forge`, `forge:build`, `forge:sim`

---

## 5. Fase lanjutan

4. **Worker + VirtualCableTransport** — host `nema.wasm` (Plan 35) di Web Worker;
   transport postMessage ⇄ PLP. *(butuh `nema.wasm` dari Plan 35 fase 4 → emsdk.)*
5. **RemoteSession + cutover /simulator** — bungkus codec+transport; pindahkan
   `/simulator` dari `sim.svelte.ts`(native) ke `RemoteSession(VirtualCable)`.
   Verifikasi paritas → **hapus `SimManager` + `packages/simulator`**.
6. **/remote + LinkHub** — `BleTransport` (Web Bluetooth) + pilihan transport
   "Simulator (WASM)". Pair (Plan 34) → handshake → stream. Reuse komponen Fase 3.
7. **/flash** — esptool-js (Web Serial) flashing + console.
8. **OTA registry** — tRPC `firmware.*` (list/serve `.bin`) + UI update.

Fase 4–6 butuh `nema.wasm` (Plan 35) & emsdk; Fase 6 BLE butuh hardware/Chrome.

---

## 6. Acceptance criteria

**Sudah tercapai (Fase 1–3)** — lihat §4. ✅

**WASM / Simulator**
- [ ] `nema.wasm` (Plan 35) jalan di Web Worker; `/simulator` render via VirtualCableTransport
- [ ] Paritas penuh dgn versi native (panel, kontrol, build) → `packages/simulator` & `SimManager` dihapus
- [ ] EXT: pause/step/time-scale (WASM-only)

**Remote**
- [ ] `/remote` connect ke device via Web Bluetooth: pair+handshake → layar HP = layar device
- [ ] `/remote` pilih "Simulator (WASM)" → me-remote instance WASM yang sama (`/simulator` & `/remote` sinkron)
- [ ] Input dari `/remote` menggerakkan target (device atau sim)

**Flash & OTA**
- [ ] `/flash` mem-flash firmware via Web Serial + console jalan
- [ ] tRPC `firmware.*` list/serve `.bin`

**Cross-cutting**
- [ ] `/simulator` & `/remote` memakai komponen `RemoteSession` yang sama
- [ ] `nema.wasm` = static asset (tanpa API); API hanya OTA registry

---

## 7. Non-Goals (v1)
- **WASM build / PLP codec C++ / RemoteService** — Plan 35 (device-side).
- Cross-device remote-of-sim (HP ke sim laptop) — butuh server WS relay; future.
- Multi-client > 2 simultan, video/grayscale stream, semantic mirroring.
- UI "USB Mass Storage mode" (suruh device expose microSD MSC) — future (Plan 34 MSC).
- Auth/akun/cloud sync.
- Firefox (`/remote`+`/flash` butuh Web Bluetooth/Serial → Chrome/Edge). `/simulator` WASM jalan di semua browser.

---

## 8. Hubungan dengan plan lain
- **35** menyuplai: PLP (kontrak codec), `nema.wasm`, semantik channel/handshake.
- **34** menyuplai semantik pairing (Web Bluetooth ↔ LE Secure) & USB.
- **33** BoardProfile JSON (via SYSTEM channel) → render device mockup + map klik→Action.
- **10/09** simulator lama = sumber migrasi (sudah dipindah; native bridge dihapus saat WASM cutover).
- **27/29** pointer dari Forge = `sendPointer` → device `postPointer` (identik touch).
