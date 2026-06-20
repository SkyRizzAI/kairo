# 75 — Network Link Transport (PLP over WebSocket) — Forge Web

> Bawa PLP ke **jaringan** lewat `ILinkTransport` baru berbasis **WebSocket**, sehingga
> **Forge web (browser)** bisa meremote device lewat WiFi — bukan cuma USB/BLE. Transport
> ini **hanya boleh hidup setelah auth (plan 74) ada** (ia mengekspos PLP ke LAN).
>
> **Scope plan ini = Forge web saja.** Forge CLI belum ada — konsumen Node (CLI), raw TCP,
> dan mDNS-browse ditangani nanti di **plan 78** (Forge CLI). Di sini cukup: device jadi WS
> server + Forge web bisa connect.

- Status: 🔴 Not started
- Depends on: **74 (Remote auth)**, 72 (NetStatus `isOnline`), 35 (PLP/ILinkTransport), 36 (Forge web)
- Blocks: Forge web remote-over-network; (plan 78 Forge CLI menyusul reuse transport ini)

---

## 0. Konteks & kenapa WebSocket

PLP sudah transport-agnostik (`ILinkTransport`): `BleLinkTransport`, `UsbCdcLinkTransport`,
`MuxTransport`, WASM virtual cable. **Belum ada transport jaringan.**

Kunci desain: **browser tidak bisa membuka raw TCP socket** (hanya WebSocket/HTTP/WebRTC).
Karena targetnya **Forge web ikut bisa remote via network**, transport jaringan = **WebSocket**:

- **Forge web (browser)** → `WebSocket` native → device. ✅
- Device jadi **WS server sendiri** (`esp_http_server`) — tetap **tanpa server perantara**,
  sesuai filosofi Forge.

PLP frame dibungkus dalam **WebSocket binary message**; RemoteService tak berubah — sama
seperti BLE/USB, beda byte pipe saja. Justru karena mudah diekspos, auth (74) wajib lebih dulu.

> Nanti (plan 78) transport WebSocket yang sama dipakai Forge CLI via Node `ws`; raw TCP
> opsional untuk klien low-level juga dibahas di sana. **Tidak** dikerjakan di plan ini.

---

## 1. Goals

- [ ] `WsLinkTransport` (core interface + impl esp32) — PLP frames di dalam WS binary frames.
- [ ] Device jalan **WebSocket server** (`esp_http_server` + WS handler) di port tetap.
- [ ] **Forge web** bisa connect via `ws://<device>.local:<port>/plp` (browser native) → panel
      remote yang sama (screen mirror / logs / cli / files).
- [ ] Listener digate **`Remote Enabled` + `isOnline()`** — bind hanya saat online & remote on;
      unbind saat WiFi putus / remote off.
- [ ] Masuk ke `MuxTransport` bersama USB/BLE.
- [ ] Device advertise **mDNS `_palanu._tcp`** (TXT: name/board/fwver/path/authrequired) — supaya
      hostname `.local` resolvable & siap untuk discovery CLI nanti.
- [ ] Tier privileged tetap terkunci sampai auth (plan 74) — diuji eksplisit.

**Non-goal (→ plan 78 Forge CLI):** transport Node `ws`, raw TCP, mDNS **browse** dari klien,
login token CLI. **Non-goal lain:** TLS/`wss://` (future hardening), WAN/relay, multi-session (45).

---

## 2. Desain

### 2.1 Device — WebSocket server (`esp_http_server`)
- `Esp32WsTransport` (platform esp32): `httpd_start()` + URI handler `GET /plp` dengan
  `is_websocket=true`. Terima `httpd_ws_recv_frame` (binary) → feed byte ke `FrameParser`
  (toleran-noise, resync `0xAB`) → `LinkService`. TX: `httpd_ws_send_frame_async`.
- Port tetap **8477** (configurable). Satu handler = satu sesi (single client dulu;
  multi-session = plan 45, di luar scope). Jalankan di task `esp_http_server` sendiri — jangan
  blok UI/main loop.

### 2.2 Lifecycle (digate, bukan selalu nyala)
```
Remote.Enabled && isOnline()            → start httpd + WS handler
WiFi disconnected (NetworkDisconnected) → stop httpd, tutup koneksi
Remote.Enabled = off                    → stop listener
```
- Subscribe `NetworkConnected/Disconnected` (plan 72) + toggle Remote (plan 74).
- `NetStatusService.isOnline()` = gate tunggal (plan 72 §3).

### 2.3 Hostname & mDNS
- esp32: set hostname (`esp_netif_set_hostname`, mis. `skyrizz-e32`) + `mdns_service_add(
  "_palanu","_tcp",8477)` + TXT `{name, board, fwver, path:"/plp", authrequired:"1"}`.
- **Forge web**: browser tak bisa mDNS-browse, tapi OS me-resolve hostname `.local` → user
  masuk `skyrizz-e32.local` (atau IP), Forge buka `ws://…:8477/plp`. (mDNS **browse** dari
  klien = plan 78.)

### 2.4 Keamanan (interaksi dgn plan 74)
- Koneksi WS baru mulai **Unauthenticated** → hanya tier observation (Screen/Log/Event/GetInfo).
  CLI/File/OTA → `AuthRequired` sampai `AuthResponse` benar.
- **Tak ada bonding** di jaringan → password/`sessionToken` per sesi (handshake identik lintas
  transport; logikanya di Forge web `RemoteSession`, nanti dipindah ke `@palanu/link` plan 77).
- TXT `authrequired=1` → Forge prompt password lebih awal (UX).
- WS beda-origin dari browser itu normal (tak ada COEP/CORS block untuk WS message); device
  boleh cek `Origin` longgar.

## 3. Forge web — konsumen

- Tambah `WebSocketTransport` di Forge web (`packages/forge/src/lib/transport/`) — `WebSocket`
  browser, binary, implement antarmuka transport yang sama dengan Ble/Serial/VirtualCable.
- `/remote`: tambah opsi **"Network (Wi-Fi)"** — input `host.local`/IP → connect → handshake
  auth (plan 74) → panel sama (screen/cli/logs/files). **Inilah yang membuat Forge web bisa
  remote via network.**
- (Transport ini ditulis di Forge web dulu; saat plan 77 ekstrak `@palanu/link`, ia pindah ke
  shared lib supaya Forge CLI reuse — tapi itu plan 77/78, bukan sekarang.)

### Verifikasi tanpa hardware
- Logika transport bisa diuji dengan **skrip Node `ws` sekali-pakai** / host test PLP (bukan
  produk CLI) untuk membuktikan frame masuk-keluar benar sebelum uji `esp_http_server` nyata.

## 4. Tasks
- [ ] `Esp32WsTransport` (esp_http_server WS handler) — RX→FrameParser, TX async.
- [ ] Wiring `Esp32Platform::postRegister` → tambah ke `MuxTransport`; `caps::RemoteNet`.
- [ ] Lifecycle gating: subscribe NetStatus (72) + Remote toggle (74).
- [ ] Hostname `.local` + mDNS advertise `_palanu._tcp` + TXT.
- [ ] Forge web: `WebSocketTransport` + opsi "Network" di `/remote` + alur auth (74).
- [ ] Skrip uji Node `ws`/host PLP minimal untuk verifikasi frame.
- [ ] Build hijau (esp32 target) + Forge web tak regresi (BLE/USB/sim tetap jalan).

## 5. Acceptance criteria
- [ ] Device online → hostname `.local` resolvable; **Forge web** connect via
      `ws://host.local:8477/plp` → screen mirror + logs tampil.
- [ ] Screen/log/event over network jalan **tanpa** auth (tier observation).
- [ ] CLI/File/OTA over network **ditolak** sampai auth (plan 74) lolos — diuji eksplisit di Forge web.
- [ ] WiFi putus → server ditutup, koneksi lepas; reconnect → listen lagi otomatis.
- [ ] `Remote Enabled = off` → port tak terbuka sama sekali (cek `nmap`/`netstat`).
- [ ] Tak ada blocking di main/UI thread; server jalan di task-nya sendiri.
- [ ] HW: diverifikasi di skyrizz-e32 (butuh WiFi jalan dari plan 73) — Forge web konek via WiFi.
