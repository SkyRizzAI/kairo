# 95 — Device as Remote Client (device↔device + remote ke simulator)

> Sekarang device cuma bisa jadi **server** remote (di-remote oleh Forge web/CLI). Plan ini
> membalik arah: device juga bisa jadi **client** — sebuah **app "Remote"** di device yang
> menampilkan layar device lain (atau simulator forge:web) dan meneruskan input ke sana.
> Protokol tetap **PLP** (transport-agnostik & simetris). Yang baru: **sisi client PLP di
> firmware** (selama ini cuma ada di TypeScript `@palanu/link`).
>
> **Scope v1 = WiFi/WebSocket saja.** BLE-central ditunda (lihat §6). Transport pertama yang
> diremote = **device lain** (sudah jadi WS server dari plan 75). Remote ke **simulator
> forge:web** butuh relay kecil (§4) dan jadi **Phase 3**.

- Status: 🔴 Not started
- Depends on: **35 (PLP/ILinkTransport)**, **75 (WS transport + mDNS device)**, **77 (`@palanu/link` — RemoteSession referensi)**, 74 (Remote auth), 27 (input chord/Action), 25 (Adaptive UI), 47/86 (app model)
- Blocks: pairing device-ke-device; demo "hardware nge-remote emulator"

---

## 0. Konteks & kenapa ini "hampir gratis"

PLP sudah simetris: codec C++ (`firmware/core/src/link/plp_codec.cpp`) mirror byte-for-byte
dengan TS (`packages/link/src/codec.ts`), bahkan share test vector. `ILinkTransport` sudah
bersih di kedua sisi. Yang belum ada cuma **sisi client di firmware**:

| Komponen | Sisi server (device) | Sisi client |
|---|---|---|
| Orkestrasi sesi | `RemoteService` ✅ (firmware) | `RemoteSession` — **cuma di TS** ❌ firmware |
| Screen | `RemoteScreenTap` (encode 1-bit RLE) ✅ | decode + render — **cuma di TS** ❌ firmware |
| Transport WS | `Esp32WsTransport` = **server** ✅ | WS **client** — ❌ belum ada |

Jadi pekerjaan inti: **port `RemoteSession` (client) ke C++** + **WS client transport** +
**app UI** yang menampilkan frame remote & meneruskan input. Tidak ada perubahan protokol.

---

## 1. Goals

- [ ] `RemoteSession` versi C++ (client PLP) di core — handshake HELLO/ACK + liveness PING/PONG
      + decode channel Screen (1-bit RLE + palette) + kirim Input.
- [ ] `WsClientLinkTransport` (firmware) — **dial keluar** ke `ws://host:8477/plp`
      (esp-websocket-client). Beda dari `Esp32WsTransport` yang server.
- [ ] **App "Remote"** (app baru, sejajar BadUSB/Wallets di launcher) dengan home/menu →
      **Start Remote** → pilih transport (WiFi) → host (auto-discovery mDNS **atau** input
      manual) → connect → **layar remote full-screen + input langsung jalan**.
- [ ] **Exit chord**: tahan **▲+OK ≥1 dtk** keluar mode remote (chord sekejap tetap
      diteruskan ke remote). Hint transien saat masuk.
- [ ] **Render frame remote** ke display lokal: scale/letterbox bila resolusi beda; pakai
      palette yang dikirim remote (atau tema lokal).
- [ ] **Phase 3:** relay kecil + bridge browser supaya device asli bisa meremote **simulator
      forge:web** (lihat §4).
- [ ] Build hijau (skyrizz-e32 target + WASM) tanpa regresi sisi server remote.

**Non-goal (v1):** BLE-central (§6), multi-host simultan, audio/file/cli **dari** sisi client
(v1 cukup screen+input), TLS/`wss://`.

---

## 2. Desain — sisi firmware (client)

### 2.1 `RemoteSession` (C++)
Port dari `packages/link/src/session.ts`. State machine:
- **Connect** → kirim `Control[HELLO]`, retry tiap ~300ms sampai `ACK`/`REJECT`.
- **Ready** → minta `System[ScreenStream=on]` (opt-in, plan 88) → terima `Screen` frame.
- **Liveness** → PING tiap 3s; diam >30s = mati → kembali ke home app dengan pesan.
- **Auth (plan 74)** → kalau `authrequired`, tampilkan prompt password di app, kirim response.
- FrameParser firmware sudah ada (toleran-noise, resync `0xAB`) → reuse.
- Decode `Screen`: `[w:2][h:2][rle...]` (Compressed flag) → buffer 1-bit. Decode `System`
  palette (RGB565) → simpan untuk render.

### 2.2 `WsClientLinkTransport` (platform esp32)
- `esp_websocket_client` → dial `ws://<host>:<port>/plp`. RX binary → FrameParser. TX async.
- Implement `ILinkTransport` yang sama (`send/onRecv/isConnected/mtu`). Jalan di task sendiri,
  jangan blok UI.
- Gate: butuh `isOnline()` (plan 72). Reuse host dari mDNS browse / input user.

### 2.3 Auto-discovery (mDNS browse di device)
- Device **bisa** mDNS browse (`mdns_query_ptr("_palanu","_tcp")`, ESP-IDF) — beda dari browser
  yang tak bisa. List hasil (`name/board/host:port/authrequired` dari TXT, plan 75 §2.3).
- Fallback: input host manual (`skyrizz-e32.local` / IP) lewat keyboard on-screen.

---

## 3. Desain — App "Remote" (UX)

Patuh aturan **App UX** (CLAUDE.md): app **selalu** buka ke home/menu, swallow Activate frame
pertama, Back dari sub-screen → home, Back dari home → exit.

```
Home (ListContainer)
 ├─ Start Remote           → pilih transport → discovery/host → connect
 ├─ Saved Hosts (kalau ada)→ langsung connect
 ├─ Settings / About
Discovery screen   : list mDNS _palanu._tcp + "Enter manually"
Connecting screen  : handshake + (opsional) prompt password (plan 74)
Remote view (full) : tampilkan frame remote, FORWARD input, hint exit transien ~3s
```

### 3.1 Forward input
- `RemoteView` map input lokal → PLP `Key` (`sendKey`). Gunakan raw `Code` **hanya** untuk
  deteksi exit-chord (kasus sah "physical identity matters", CLAUDE.md Input). Sisanya lewat
  `Action`/`Key` normal yang diteruskan.

### 3.2 Exit chord (keputusan desain)
- **Tahan ▲+OK ≥ ~1 dtk → keluar** ke home app; **tidak** diteruskan ke remote.
- Chord ▲+OK **sekejap tetap diteruskan** → remote tak kehilangan kemampuan chord.
- Deteksi di `RemoteView` via timestamp tekan (wall-clock), bukan keymap global.
- **Hint transien** saat masuk: FooterLegends/toast "Tahan ▲+OK untuk keluar" ~3s; munculkan
  lagi kalau user mencet-mencet tanpa hasil (heuristik input tanpa exit).

### 3.3 Render frame remote
- Frame = 1-bit @ resolusi remote. Display lokal mungkin beda → **scale integer/letterbox**
  (resolution-independent, plan 25). Pakai palette remote (RGB565) atau tema lokal.

---

## 4. Phase 3 — Remote ke simulator forge:web (lewat relay)

**Kendala fundamental:** browser **tak bisa** jadi server WS. WASM sim jalan **di dalam
browser** lewat *virtual cable* in-process (`VirtualCableTransport.ts` ↔ `wasm_cable_transport.cpp`).
Build WASM `-sENVIRONMENT=web,worker` → **tak bisa headless di Node** (jalan buntu, jangan
ditempuh). Maka:

```
WASM sim (server) ↔ virtual cable ↔ Browser-bridge ──WS──▶ Relay ◀──WS── Device asli (client)
```

- **Relay WS kecil** (~60 baris `ws`) — nebeng Node backend SvelteKit forge yang sudah ada
  atau standalone. Cuma titik temu; cross-pipe dua client.
- **Browser-bridge** di forge:web: toggle "Expose simulator to network" → forge buka WS
  **client** ke relay, lalu jembatani byte virtual-cable (output `RemoteService` WASM) ↔ relay.
  Browser-as-WS-client **didukung penuh** → tak perlu sentuh build WASM.
- **Device asli** = `RemoteSession` client → dial `ws://<dev-machine>:<port>/plp` (relay) →
  relay teruskan ke browser-bridge → sampai ke WASM sim. Device & dev-machine harus se-LAN.

> Hasil: forge:web sim **bisa dibuka & diremote device asli** — demo "hardware nge-remote
> emulator", tanpa rebuild WASM.

---

## 5. Tasks

**Phase 1 — Client core + WS transport (device↔device)**
- [ ] `RemoteSession` C++ (handshake, liveness, decode Screen+palette, sendKey, auth).
- [ ] `WsClientLinkTransport` (esp-websocket-client) — dial keluar, FrameParser, task sendiri.
- [ ] Test host PLP (Node `ws` sekali-pakai) untuk verifikasi handshake + screen decode.

**Phase 2 — App "Remote" + UX**
- [ ] App "Remote" (home/menu, swallow first Activate) di launcher (sejajar BadUSB/Wallets).
- [ ] Discovery screen (mDNS browse `_palanu._tcp`) + input host manual.
- [ ] Connecting screen + prompt password (plan 74) bila `authrequired`.
- [ ] `RemoteView`: render frame (scale/letterbox + palette) + forward input.
- [ ] Exit chord (tahan ▲+OK ≥1s) + hint transien.

**Phase 3 — Remote ke simulator**
- [ ] Relay WS kecil (nebeng Node backend forge atau standalone).
- [ ] Browser-bridge di forge:web (toggle expose + jembatani virtual cable ↔ relay).
- [ ] Verifikasi device asli (atau dua tab forge) konek ke sim via relay.

**Umum**
- [ ] Build hijau skyrizz-e32 + WASM; sisi server remote (plan 35/75/88) tak regresi.

---

## 6. Catatan: kenapa BLE-central ditunda

Device sekarang GATT **peripheral** (server). Jadi client butuh GATT **central** —
`esp32_ble.cpp` belum mendukungnya, dan dual-role nambah kompleksitas besar. WiFi/WS sudah
cukup untuk v1 (device target = WS server dari plan 75, tinggal didial). BLE-central = plan
terpisah kalau memang butuh remote tanpa WiFi.

---

## 7. Acceptance criteria
- [ ] Dari app "Remote" device A: pilih WiFi → discovery menemukan device B (`_palanu._tcp`)
      **atau** input host manual → connect → **layar device B tampil di device A**, input device
      A mengontrol device B.
- [ ] Tahan ▲+OK ≥1s → keluar ke home app; chord sekejap tetap sampai ke remote.
- [ ] Hint exit muncul saat masuk view dan hilang ~3s.
- [ ] Resolusi beda → frame ter-scale/letterbox benar (tak gepeng/terpotong).
- [ ] `authrequired=1` → prompt password; salah → ditolak; benar → masuk.
- [ ] Koneksi putus / remote off → kembali ke home app dengan pesan, bukan hang.
- [ ] **Phase 3:** device asli (atau tab forge kedua) bisa meremote simulator forge:web lewat
      relay — screen mirror + input jalan.
- [ ] HW: diverifikasi di skyrizz-e32 (butuh WiFi plan 73).
