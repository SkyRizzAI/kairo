# 88 — Remote Protocol v2 (PLP/2): Sessions, Bulk Transfer, Liveness & Auth Hardening

> **Penerus Plan 35 (PLP) + Plan 74 (auth).** Plan 35 membangun PLP v1: framing,
> channel mux, handshake, screen/input/log/cli/file/ota. Plan 74 menambah tier
> auth. Setelah dipakai sungguhan (push `.papp.zip` ke device lewat Forge CLI),
> **muncul kelas bug struktural yang tak bisa ditambal satu-satu** — semuanya
> berakar pada keputusan desain v1 yang sama. Plan ini **menaikkan protokol ke v2**
> dengan request/response berkorelasi, transfer berukuran besar yang aman & resumable,
> deteksi liveness in-protocol, model sesi multi-remote, dan auth yang diperkuat —
> **satu desain yang menyelesaikan banyak gejala sekaligus**, bukan tambal-sulam.

- Status: ✅ **Fase 1–6 selesai & HW-verified** (skyrizz-e32 + WASM build + Forge CLI). cp dua-arah (chunked write + chunked read) byte-identik termasuk file 200 KB; `exec`/shell, auth+password fallback, heartbeat. Lihat §18.
- Milestone: M9+ (Remote/Forge maturation)
- Depends on: **35 (PLP/RemoteService)**, **74 (channel tiers + session auth)**, 45 (multi-session CLI), 39 (OTA chunked — pola acuan)
- Blocks: 75 (TCP transport — tidak boleh online tanpa v2), Forge file-manager production
- Target verifikasi: **skyrizz-e32 (ESP32-S3)** + **WASM simulator** + **Forge CLI** + **Forge Web**

---

## 0. Konteks — kenapa v2, bukan tambalan

PLP v1 sehat di lapisan wire (codec C++↔TS **byte-identik**, CRC-8/SMBus, resync
di magic byte). Yang cacat adalah **cara protokol dipakai di atas wire**. Audit
menyeluruh (firmware wire-layer, RemoteService, `@palanu/link`, Forge-CLI,
Forge-Web, platform WASM) menemukan satu pola yang berulang:

> **`LinkService` di-harden untuk konkurensi (atomics + 2 mutex), tetapi
> `RemoteService` — pemilik state auth, tabel sesi, routing FILE/OTA — diasumsikan
> single-thread. Fix async `file_rx` (D5) menambah task kedua yang menyentuh state
> itu tanpa proteksi, dan reply kini melewati batas task tanpa korelasi.**

Akar dari bug `cp` yang memicu plan ini (`writeFile` ke `/sd/apps/` gagal status 2
lalu device unresponsive) **bukan SD card**, melainkan asimetri desain:

| | OTA (`pushFirmware`) | FILE write (`writeFile`/`cp`) v1 |
|---|---|---|
| Chunking | ✅ 1792 B/frame + offset | ❌ seluruh file dalam 1 frame |
| Idempotent retry | ✅ per-offset | ❌ tak ada |
| Begin→Data→End | ✅ | ❌ single-shot |

File 4.9 KB di-blast sebagai satu frame → HWCDC RX ring (device baca 256 B/iterasi)
**overflow** → frame korup (status 2) + sisa byte mencemari `buf_` parser → HELLO
berikutnya tak terparse → **device unresponsive**. OTA sendiri sudah punya komentar
*"4096 broke HW (USB-CDC RX ring overflow)"* — tim sudah tahu batas ini nyata.

### 0.1 Inventaris cacat (referensi yang diperbaiki plan ini)

Kode prefiks: **W**=wire/transport firmware, **R**=RemoteService firmware,
**S**=client shared `@palanu/link`/CLI, **F**=Forge-web. Severity: 🔴 kritikal,
🟠 tinggi, 🟡 menengah.

| ID | Sev | Cacat | Lokasi |
|---|---|---|---|
| S1/F2 | 🔴 | Write seluruh file 1 frame; wrap di 64 KB | `session.ts:406`, `cp.ts:89` |
| S2/F2 | 🔴 | Multi-KB write lampaui RX ring USB-CDC (OTA pakai 1792 B) | `session.ts:445` vs `:406` |
| S3/F1/WF1 | 🔴 | Tak ada request-id; reply dikorelasi FIFO-per-opcode → desync setelah timeout | `session.ts:264-274` |
| R1/F1 | 🔴 | State auth (`authorized_`,`nonce_`) plain, ditulis lintas-task tanpa lock | `remote_service.h:119-120` |
| R2/F2 | 🔴 | Reply FILE dikirim dari `file_rx`; koneksi bisa sudah mati/ganti sesi, tanpa session-id | `remote_service.cpp:287` |
| W1/W2 | 🔴 | `parser_.reset()` dipanggil di luar `recvMtx_` → data race `buf_`; buffer tak berbatas | `link_service.cpp:62`, `plp_codec.cpp:38` |
| W4/S5 | 🟠 | USB tak punya deteksi disconnect; `ready_` nyangkut; `#helloTimer` bocor | `esp32_usb_cdc.cpp`, `session.ts:108` |
| R3/R4 | 🟠 | `file_rx` single-thread HOL; queue depth-4 silent-drop | `esp32_platform.cpp:327` |
| R9 | 🟠 | OTA jalan inline di `cdc_rx` (tak di-defer) → blok handshake | `remote_service.cpp:196` |
| R8 | 🟠 | `handleFile` & CLI-FS jalan di dua task atas FS yang sama tanpa mutex | `remote_service.cpp:178,191` |
| S4 | 🟠 | `waitForAuthorized` menelan AUTH_FAIL di gap pasca-ready | `forge-cli/session.ts:77` |
| F4 | 🟠 | Slot device tunggal ditimpa tanpa close transport lama | `remoteLink.ts:24` |
| F5 | 🟠 | Token auth localStorage satu key global, ditimpa antar-device | `tokens.ts:20`, `session.ts:233` |
| R5/R6 | 🟡 | Read/Copy buffer seluruh file di RAM (3× copy); status 2 tanpa errno (LittleFS) | `remote_service.cpp:306`, `littlefs_filesystem.cpp:77` |
| R7 | 🟡 | LittleFS `list` tak feed watchdog → reset di dir besar (SD feed, LittleFS tidak) | `littlefs_filesystem.cpp:43` |
| S6 | 🟡 | Pre-check SD pakai timeout 5 s → false-negative "SD not mounted" | `cp.ts:78` |
| W7/F7 | 🟡 | Tak ada penanganan short-write; write "sukses" bisa file korup | `link_service.cpp:101` |
| F6 | 🟡 | `ws://` tanpa TLS — auth+file+CLI cleartext di LAN | `WebSocketTransport.ts:24` |
| S10 | 🟡 | `Flags.FragMore` didefinisikan tapi tak pernah dipakai | `codec.ts:27` |
| W9/F10 | 🟡 | `rleDecode` tanpa batas output; screen frame tak divalidasi vs `w*h` | `plp_codec.cpp:71` |

**Catatan sehat (tidak diubah):** codec wire TS↔C++ byte-identik; CRC; resync.
v2 **kompatibel-mundur di lapisan frame** (magic+crc tetap), perubahan ada di
lapisan *message* di atasnya, dengan negosiasi versi di HELLO.

---

## 1. Prinsip desain (cara sistem remote produksi bekerja)

v2 mengadopsi pola dari SSH, HTTP/2, gRPC, Chrome DevTools Protocol (CDP), LSP, dan
mosh — disesuaikan ke embedded (ESP32-S3) dan WASM single-inline:

1. **Korelasi via request-id, bukan urutan.** Tiap request bawa `reqId` monotonik;
   reply meng-echo-nya. Map `reqId→resolver`, bukan FIFO. (JSON-RPC `id`, CDP `id`,
   HTTP/2 stream-id, LSP `id`.) → memperbaiki S3/F1/WF1.
2. **Multipleks logis di atas satu koneksi.** Channel sudah ada; tambah **sesi**
   sebagai unit otorisasi+korelasi. (SSH channels, HTTP/2 streams.)
3. **Transfer besar = chunk berbatas + offset + ack (flow control).** Tak pernah
   buffer seluruh payload; receiver men-dikte `chunkSize` aman. (HTTP/2
   `SETTINGS_MAX_FRAME_SIZE` + window; SSH window-adjust; rsync; OTA repo ini.)
   → memperbaiki S1/S2/F2.
4. **Liveness in-protocol.** Heartbeat PING/PONG + timeout mendeteksi link mati
   walau transport (USB/WASM) tak memberi sinyal disconnect. (WebSocket ping, SSH
   keepalive, gRPC HTTP/2 ping.) → memperbaiki W4/S5.
5. **Auth state-machine sebelum privileged, challenge-response, token per-sesi
   ber-scope + expiry.** (SSH auth, OAuth bearer dengan audience.) → R1/F5/S4.
6. **Negosiasi versi + fitur di handshake.** HELLO bawa `protoVersion` + flag fitur;
   kedua sisi sepakati `maxFrameSize`, `chunkSize`. (TLS version, ALPN.)
7. **Buffer berbatas + frame ceiling keras.** Parser tolak `len` > ceiling →
   resync, bukan numpuk 64 KB. (HTTP/2 max-frame.) → W1/W9/F11.
8. **Transfer idempotent & resumable.** Write/read berbasis offset; lanjut setelah
   reconnect. (rsync, OTA.) → R2.
9. **Model error terstruktur.** Kode status spesifik (NotMounted, NoSpace,
   TooManyOpenFiles, …), bukan "status 2" buta. → R5/R6/S6.
10. **Kontrak konkurensi dwi-mode.** Handler **wajib non-blocking atau chunked**
    agar benar saat inline (WASM page-thread) maupun deferred (ESP32 `file_rx`).
    → R3/R4/R8/R9 + constraint WASM.

---

## 2. Goals

1. **Device bisa di-remote penuh lewat Forge Web & Forge CLI**, identik di
   **ESP32-S3** dan **WASM simulator** (satu protokol, banyak transport).
2. **Melayani semua bentuk remote dengan benar di bawah beban & reconnect:**
   - **Remote display** — screen-stream RLE, frame divalidasi, tahan-noise.
   - **Remote input** — channel Input (sudah baik; queue-based ke GUI thread).
   - **Remote CLI** — multi-sesi shell (Plan 45) dengan korelasi reqId.
   - **Remote files** — list/read/write/mkdir/remove/rename/copy via **bulk
     transfer chunked resumable**; tak ada batas 64 KB; tak overflow RX.
3. **Model sesi** mengatur korelasi + otorisasi + lifecycle; **multiple remote**
   (USB + BLE + WS bersamaan) ditangani benar.
4. **Sistem auth bekerja & aman** — challenge-response, token per-device
   ber-scope+expiry, state ter-lock, gagal-aman.
5. **Sistem connect & handler multi-remote** — deteksi liveness in-protocol,
   reconnect dengan resume, cleanup in-flight saat drop.
6. **Semua cacat di §0.1 tertutup**, dengan matriks keterlacakan (§13).

Non-goal (plan terpisah): TLS/`wss` untuk transport jaringan (catat di §9.4,
implementasi penuh menyusul Plan 75); kompresi selain RLE screen.

---

## 3. Arsitektur v2 (lapisan)

```
┌──────────────────────────────────────────────────────────────┐
│ Aplikasi remote: Display · Input · CLI · FileManager · OTA     │
├──────────────────────────────────────────────────────────────┤
│ MESSAGE LAYER (BARU v2)                                         │
│  · Envelope {reqId, msgType}  · Bulk-transfer (Begin/Data/End) │
│  · Session model  · Auth state-machine  · Error codes          │
├──────────────────────────────────────────────────────────────┤
│ LINK LAYER (v1, diperketat)                                    │
│  · LinkService: handshake, channel gate, heartbeat            │
│  · FrameParser: frame ceiling, reset-in-lock                  │
├──────────────────────────────────────────────────────────────┤
│ FRAME LAYER (v1, kompatibel)                                   │
│  · [0xAB][chan][flags][len:2][payload][crc8]  · negosiasi size │
├──────────────────────────────────────────────────────────────┤
│ ILinkTransport: USB-CDC(HWCDC) · BLE · WebSocket · WasmCable   │
└──────────────────────────────────────────────────────────────┘
```

**Pembagian inline-vs-deferred (kunci dwi-platform):**
- **WASM**: seluruh jalur RX jalan inline di page-thread; FS = MemFS (instan).
  Karena bulk-transfer **chunked**, tiap chunk cepat → aman inline, tak membekukan
  UI browser. `setFileDeferred` tetap tak dipasang (jalur inline).
- **ESP32**: `cdc_rx` (prio 5) hanya parse+enqueue; `file_rx` (prio 3) eksekusi
  chunk. Karena tiap chunk kecil & idempotent, queue-full → drop+retry aman.

---

## 4. Frame layer — pengetatan (kompatibel)

Wire tetap `[0xAB][chan:1][flags:1][len:2 LE][payload][crc8]`. Perubahan:

1. **Frame ceiling keras.** Parser punya konstanta `PLP_MAX_FRAME` (default 2048,
   ≥ `maxFrameSize` ternegosiasi). Jika `len` > ceiling → **buang 1 byte & resync**
   (jangan tunggu sampai 64 KB). Hilangkan W1/W9/F11. Terapkan di **`plp_codec.cpp`
   dan `codec.ts` bersamaan** (kontrak bersama).
2. **`reset()` selalu di dalam `recvMtx_`.** `LinkService::handle(HELLO)` tak lagi
   memanggil `parser_.reset()` langsung; reset dilakukan di `onBytes` di bawah lock,
   atau via flag yang diproses parser saat push. Hilangkan W2.
3. **Negosiasi `maxFrameSize`/`chunkSize`** dibawa di HELLO/ACK (lihat §5.1).
   Default device ESP32: `maxFrameSize=1024`, `chunkSize=1024` (aman < RX ring
   256 B per-read tapi total ring cukup; diverifikasi di HW). WASM: 4096 (in-proc).
4. **Short-write ditangani.** `ILinkTransport::send` mengembalikan jumlah byte;
   `LinkService::send` retry sisa (loop) dan/atau laporkan gagal. Hilangkan W7.
   (Fragmentasi MTU tetap di layer message, bukan frame.)

---

## 5. Message layer — envelope & korelasi reqId

Channel **request/response** (`File`, `Cli`, `Ota`, `System`) memakai envelope
seragam sebagai byte awal payload:

```
[msgType:1][reqId:2 LE][payload op-specific...]
```

- `reqId`: monotonik per-sesi, **dialokasikan host** untuk request; device
  **echo** di reply. Channel broadcast (Screen/Log/Event) **tidak** pakai reqId.
- Client: ganti `#filePending: Record<op, resolver[]>` → `#pending: Map<reqId,
  {resolve, timer, op}>`. Reply di-route by reqId, bukan `.shift()`. Hilangkan
  S3/F1/WF1. Reply tak dikenal (reqId asing / sudah timeout) → **diabaikan**, tak
  meracuni request lain.
- Firmware: `RemoteService::reply()` menyertakan `reqId` dari request. Bila reply
  tiba setelah sesi berganti (R2), reqId tak cocok di host → diabaikan dengan aman.

### 5.1 Handshake v2 (negosiasi)

```
Host → HELLO   [protoVersion:1][featureFlags:2][maxFrameSize:2]
Dev  → ACK     [protoVersion:1][featureFlags:2][maxFrameSize:2][chunkSize:2][sessionId:2]
              (lalu, bila password diset) AUTH_CHALLENGE [salt:nonce]
```

- `protoVersion`: device dukung v1 (legacy) **dan** v2. Jika host kirim v1 HELLO
  (1 byte), device balas ACK v1 → kompatibel-mundur. Forge baru kirim v2.
- `sessionId`: id sesi device (untuk multi-remote & validasi reply lintas-reconnect).
- `featureFlags`: bit untuk `bulkTransfer`, `heartbeat`, `structuredErrors`.

---

## 6. Bulk transfer — sub-protokol terpadu (file + OTA)

Generalisasi pola OTA (Begin/Data/End) jadi mekanisme transfer yang dipakai
**File write, File read, dan OTA**. Menggantikan `writeFile` single-shot.

### 6.1 Write (host → device)

```
Host → XFER_BEGIN [reqId][mode:1][path\0][totalSize:4]
Dev  → XFER_READY [reqId][status][xferId:2][chunkSize:2]      # device dikte chunk aman
Host → XFER_DATA  [reqId][xferId][offset:4][bytes...]         # offset-based, idempotent
Dev  → XFER_ACK   [reqId][xferId][offset:4][status]           # ack per chunk (flow control)
... ulang sampai totalSize ...
Host → XFER_END   [reqId][xferId][crc32:4]
Dev  → XFER_DONE  [reqId][xferId][status]                     # commit + verifikasi crc
```

- `chunkSize` **didikte device** (ESP32: 1024; WASM: 4096) — host tak boleh
  melampaui. Hilangkan S1/S2/F2 secara struktural.
- **Idempotent**: device tulis pada `offset`; chunk diulang (retry) aman. Resume
  setelah reconnect = lanjut dari `offset` ter-ack terakhir (R2).
- **Backpressure**: host kirim chunk berikut hanya setelah `XFER_ACK` (window=1
  untuk embedded; bisa dinaikkan via featureFlag). Aman inline di WASM.
- File ditulis streaming (append per chunk) — **tidak** buffer seluruh file di RAM
  (hilangkan R5 untuk write; lihat §6.3 untuk read).

### 6.2 Read (device → host)

```
Host → XFER_BEGIN [reqId][mode=READ][path\0]
Dev  → XFER_READY [reqId][status][xferId][chunkSize][totalSize:4]
Dev  → XFER_DATA  [reqId][xferId][offset][bytes...]   # device stream
Host → XFER_ACK   [reqId][xferId][offset]             # host flow-control
... sampai totalSize ...
Dev  → XFER_DONE  [reqId][xferId][crc32]
```

### 6.3 OTA disatukan

OTA jadi `mode=FIRMWARE` di mekanisme yang sama (atau tetap channel `Ota` yang
memakai envelope+bulk yang sama). Menghilangkan duplikasi & R9: OTA write dijalankan
**chunk-per-chunk lewat jalur deferred yang sama** seperti FILE, sehingga `cdc_rx`
tak diblok flash-erase. (Saat ini OTA inline di `cdc_rx`.)

### 6.4 Operasi non-bulk (tetap satu round-trip)

`list`, `mkdir`, `remove`, `rename`, `copy`, `stat` tetap request/reply tunggal
**dengan envelope reqId**. `copy` di device dilakukan **streaming src→dst** (tak
baca seluruh file ke RAM) → hilangkan R5 untuk copy.

---

## 7. Model sesi & multiple remote

### 7.1 Objek Session (device)

`RemoteService` memelihara tabel sesi kecil (pool tetap, mis. `MAX_SESSIONS=3`):

```cpp
struct RemoteSession {
    uint16_t id;
    bool     authorized = false;     // tier privileged?
    std::string nonce;               // challenge aktif
    uint16_t nextReqIdEcho;          // (opsional) validasi
    uint32_t lastSeenMs;             // liveness
    // sub-sesi CLI (Plan 45) tetap di CliSessionManager, dikunci ke session.id
    FsTransfer activeXfer;           // state bulk-transfer berjalan (offset, fd)
};
```

- **Seluruh state mutable RemoteService dilindungi `std::mutex stateMtx_`** (auth,
  nonce, tabel sesi, activeXfer). Hilangkan R1/R8/F12. Real-lock di ESP32 (multi-task)
  dan WASM (page-thread vs GUI-thread).
- **Broadcast vs privileged**: Screen/Log/Event di-stream ke **semua observer**
  (read-only, tier observation Plan 74). File/CLI/OTA/System per-sesi & butuh
  `authorized`.

### 7.2 Multiple remote (USB + BLE + WS)

Dua opsi; plan memilih **B** untuk embedded:

- **A. Satu LinkService per transport** (parser+state independen) — paling bersih
  (a la HTTP server per-connection) tapi mahal di RAM ESP32.
- **B. Satu LinkService, MuxTransport, multi-Session di RemoteService.** Parser
  tetap satu tetapi **reset-in-lock** (W2 fixed) dan tiap frame membawa `sessionId`
  pasca-handshake sehingga reply/route benar per-sesi. Lebih hemat; cukup untuk
  2–3 remote simultan. Heartbeat per-sesi mendeteksi yang mati.

> Saat ini hanya 1 remote efektif (Forge-web `remoteLink.current` slot tunggal,
> F4). v2 memperbaiki sisi device agar **mampu** banyak; Forge-web boleh tetap
> 1-aktif di UI tapi **wajib close transport lama saat ganti** (F4).

---

## 8. Liveness, connect & reconnect

### 8.1 Heartbeat in-protocol (mengatasi "USB tak punya disconnect")

- **Host** kirim `PING` tiap 2 s bila idle; **device** balas `PONG` + update
  `lastSeenMs`.
- **Device**: task pengawas (atau cek di GUI tick) — bila `now - lastSeenMs >
  6 s` → `markDisconnected(session)`: tutup `activeXfer`, drop sub-sesi CLI, reset
  `authorized`, **reset parser di dalam lock**. Hilangkan W4 + R-bersih.
- **Host**: bila tak ada `PONG` 6 s → anggap mati → hentikan `#helloTimer`
  (hilangkan S5), gagalkan semua `#pending` dengan error `Disconnected`, emit event
  ke UI.

### 8.2 Reconnect dengan resume

- Host reconnect → HELLO baru → sessionId baru. Bila ada `activeXfer` yang
  tertinggal, host boleh `XFER_BEGIN` ulang dengan `resumeFrom=lastAckedOffset`
  (idempotent §6.1). File-manager melanjutkan, bukan mengulang dari 0.
- Forge-web: tambah reconnect backoff untuk WebSocket/BLE (WF3) — opsional di UI,
  tapi sesi & `#pending` cleanup wajib (sudah di §8.1).

### 8.3 Cleanup in-flight saat drop (client)

`onState(false)` / heartbeat-timeout **wajib**: clear `#helloTimer`, batalkan
semua timer `#pending`, resolve `null`/`Disconnected`, reset `#pendingChallenge`
(S11), reset `#authorized/#ready`.

---

## 9. Auth diperkuat

Pertahankan challenge-response sha256 (Plan 74, sudah benar secara kripto:
`sha256(sha256(salt+pw)+nonce)`). Perbaikan:

1. **State ter-lock** (§7.1): `authorized`/`nonce` di bawah `stateMtx_`. Hilangkan
   R1. `onDisconnect`/`RemoteToggled` tak lagi race `dispatch`.
2. **`waitForAuthorized` gagal-aman** (S4): pasang listener `authfail` **sebelum**
   menunggu `ready`, atau cek state authfail yang sudah ter-set. Jangan resolve
   diam-diam di timeout saat password salah; bedakan "firmware lama tanpa AUTH_OK"
   (timeout→lanjut) dari "AUTH_FAIL diterima" (reject).
3. **Token per-device** (F5): Forge-web simpan token di key
   `palanu.remote.token.<deviceId>`, bukan satu key global. AUTH_FAIL device-B tak
   menghapus token device-A. Tambah `expiry` di token; device tolak token kedaluwarsa.
4. **TLS/`wss`** (F6) — **out of scope penuh**, tapi v2 menandai transport
   jaringan sebagai "insecure" di UI Forge sampai Plan 75 menambah `wss`. Channel
   privileged via TCP tetap ditolak sampai TLS ada (selaras Plan 74 §blocks 75).
5. **Parsing challenge robust** (F8): pisah `salt:nonce` dengan batas/validasi;
   bila malformed → error eksplisit, bukan diam.

---

## 10. Model error terstruktur

Ganti `status 2` buta. Definisikan enum bersama (C++ + TS) di `FileStatus`:

```
0 Ok · 1 NotFound · 2 NotMounted · 3 NoSpace · 4 TooManyOpenFiles
5 PermissionDenied · 6 IoError · 7 BadRequest · 8 Unauthorized
9 NotADirectory · 10 AlreadyExists · 11 CrcMismatch · 12 Timeout
```

- Backend FS map errno → kode (SD & **LittleFS keduanya log errno**, hilangkan R6).
- `XFER_DATA` di-`stat` ENOSPC → `NoSpace`; `fopen` EMFILE → `TooManyOpenFiles`
  (cap `max_files=5` SD jadi terdiagnosa, R6). Host tampilkan pesan manusiawi.
- **Watchdog**: `littlefs_filesystem.cpp::list` panggil `esp_task_wdt_reset()`
  seperti SD (hilangkan R7). Listing besar tak me-reset device.

---

## 11. Kontrak konkurensi (ESP32 ⇄ WASM)

Aturan yang membuat satu kode benar di kedua platform:

1. **Handler request tak boleh blocking lama.** Bulk-transfer chunked menjamin
   tiap unit kerja kecil → aman inline (WASM page-thread) & deferred (ESP32).
2. **Seam `setFileDeferred` tetap opsional.** WASM: tak dipasang → inline. ESP32:
   dipasang → `cdc_rx` enqueue, `file_rx` eksekusi. **OTA ikut seam yang sama**
   (hilangkan R9).
3. **Semua state RemoteService di bawah `stateMtx_`**; `LinkService` tetap pakai
   `sendMtx_`/`recvMtx_` + atomics. Mutex **nyata** di kedua platform (WASM =
   pthread/Worker).
4. **Queue `file_rx` membawa reqId+sessionId**; drop saat penuh **aman** karena
   chunk idempotent → host retry by offset (R3/R4 jadi tak fatal).
5. **`activeXfer` per-sesi**, di-tutup saat disconnect/timeout (tak ada fd bocor).

---

## 12. Fase & checklist

Urut agar **bug `cp` Anda pulih paling awal** (Fase 1–2), lalu pengerasan.

### Fase 1 — reqId korelasi + codec safety (🔴 S3/F1/WF1, W9/F10) ✅
- [x] Envelope FILE `[op][reqId:2]` (request) / `[op][reqId:2][status]` (reply);
      firmware echo reqId (`remote_service.cpp`), reply tanpa path-echo.
- [x] Client `@palanu/link`: `#filePending: Map<reqId>`; route by reqId; reply asing
      diabaikan. Forge-web & CLI ikut otomatis (lib bersama).
- [x] `rleDecode` ber-batas output (C++ + TS); validasi screen `w*h` + dimensi
      masuk akal sebelum alokasi.
- [ ] (ditunda) Frame ceiling `PLP_MAX_FRAME`: nilai marginal karena `len` sudah
      16-bit dan screen frame sah bisa mendekati 64 KB; reqId di Cli/System.

### Fase 2 — Chunked write + HW root-cause (🔴 S1/S2/F2) — **`cp` PULIH & HW-verified** ✅
- [x] Firmware: `WriteBegin/WriteData/WriteEnd`, `chunkSize` didikte device (1024),
      idempotent by offset, buffer-RAM lalu `fs_->write` di `WriteEnd`.
- [x] `@palanu/link`: `writeFile` chunked + ack-paced + retry idempotent; hapus
      path single-shot 64 KB. (`readFile` masih single-shot — chunk read ditunda.)
- [x] `cp.ts`: pre-check SD jadi peringatan lunak (S6), bukan throw.
- [x] **Verifikasi skyrizz-e32: `cp .papp.zip → /sd/apps/` sukses 306 ms, ulang
      3× tanpa reset, device tetap responsif, file utuh di SD.**

> **Temuan hardware (akar sebenarnya, di luar dugaan plan awal).** Chunking + reqId
> saja TIDAK cukup; tiga sebab fisik baru ketahuan saat uji di device dan ikut
> diperbaiki di Fase 2:
> 1. **HWCDC RX queue default 256 B** (`esp32_usb_cdc.cpp`). ISR menyuap FIFO 64 B
>    byte-per-byte; satu frame ~1 KB meluap queue saat `cdc_rx` sempat dipreempt →
>    byte hilang → CRC gagal → frame tak pernah diproses. **Fix: `setRxBufferSize(4096)`
>    + read buffer 256→1024.** Inilah penyebab utama `cp` gagal/`status 2` semu.
> 2. **Async `file_rx` deferral (D5) ternyata regresi.** Di prioritas rendah task ini
>    di-starve WiFi/GUI dan tak pernah menguras antrian → file op time-out diam-diam
>    (Forge Web dulu jalan justru karena saat itu file op masih **inline**). **Fix:
>    hapus deferral, jalankan file op INLINE di `cdc_rx` — sama seperti WASM.** Aman
>    karena chunked: per-frame cuma memcpy; hanya `WriteEnd/List/Read` sentuh disk.
> 3. **Screen-mirror membanjiri link & men-starve jalur file/CLI.** **Fix: mirror jadi
>    opt-in per sesi** (`SysOp::ScreenStream`, default OFF, reset tiap HELLO).
>    `@palanu/link` auto-subscribe hanya bila ada listener `screen` → Forge Web tetap
>    dapat layar, CLI tak kebanjiran. (Ini juga sebagian dari sasaran §7 "sesi
>    mengaturnya".)
> 4. **Auth S4 terkonfirmasi nyata**: token sesi di-invalidate saat device reboot;
>    `waitForAuthorized` lalu lanjut diam-diam (authorized=false) → semua File op
>    di-drop device. Password (`Q`) sebagai fallback memulihkan; perbaikan tuntas S4
>    dijadwalkan **Fase 5**.

### Fase 2.5 — Hotfix korektitas (🔴 cacat yang DIPERKENALKAN Fase 1–2) — **kerjakan DULU**
> Review adversarial atas kode Fase 1–2 menemukan bug baru. Harus ditutup sebelum
> menumpuk fitur di atasnya. Rinci di §17.
- [ ] **N1 (🔴 crash): `cdc_rx` 8 KB stack kini jalankan FAT/LittleFS scan INLINE.**
      Naikkan stack `cdc_rx` ke 12 KB (seperti `file_rx` dulu) + pindahkan `buf[1024]`
      keluar dari stack (static/heap) + `uxTaskGetStackHighWaterMark` cek di list besar.
- [ ] **N2 (🔴 korupsi): `handleFile` abaikan `reqId` saat WriteData/WriteEnd.**
      Validasi `reqId == xfer_.reqId`; tolak/abaikan WriteData dari transfer lain;
      WriteBegin mid-transfer → tolak (BadRequest), jangan clobber diam-diam.
- [ ] **N3 (🟠 reliabilitas): WriteBegin & WriteEnd tanpa retry; WriteEnd tak idempotent.**
      Client: retry WriteBegin/WriteEnd. Device: ingat hasil WriteEnd terakhir per
      `reqId` agar WriteEnd ulang membalas hasil yang sama (bukan BadRequest).
- [ ] **N4 (🟠): `#helloTimer` tak di-clear di `onState(false)`** (S5) + `screenWanted_`
      bisa nyangkut ON (reset hanya di HELLO, USB tanpa disconnect) → reset juga saat
      auth-loss/`markDisconnected`.
- [ ] **N5 (🟡): cek end-to-end** — client pastikan `off === data.length` sebelum
      WriteEnd (L2); device write 0-byte aman (`data()==nullptr` di tiap backend, L3).
- [ ] Bersihkan API defer mati (`setFileDeferred`/`deferFileFn_`) atau assert unset (S1).

### Fase 3 — State lock + error terstruktur (🟢 disederhanakan oleh inline; R1/W2, R6/R7)
> Inline file-ops **menyelesaikan R2/R3-lama/R4-lama/R8** (tak ada lagi `file_rx` →
> tak ada race lintas-task atas `fs_`/reply). Sisa Fase 3 mengecil:
- [ ] R1: lindungi `authorized_`/`nonce_` dari race `RemoteToggled` (event thread) vs
      `dispatch` (cdc_rx) — atomic/lock kecil.
- [ ] R6: `FileStatus` granular (NotMounted/NoSpace/TooManyOpenFiles…) dari errno
      SD **dan** LittleFS; client (CLI+web) tampilkan pesan spesifik, bukan "error".
- [ ] R7: `esp_task_wdt_reset()` di `LittleFsFileSystem::list` (SD sudah) — **kini
      kritikal karena list jalan inline di cdc_rx** (watchdog + HELLO-stall).
- [ ] W2: `parser_.reset()` hanya di dalam `recvMtx_` (penting untuk MuxTransport).

### Fase 4 — Liveness, disconnect, reconnect (🟠 W4/S11; OTA direvisi)
- [ ] Heartbeat PING/PONG + `lastSeenMs`; deteksi link mati in-protocol (USB/WASM tak
      beri sinyal) → `markDisconnected` (tutup `xfer_`, reset `screenWanted_`, auth).
- [ ] Client: pada drop, batalkan `#pending`, reset challenge (S11 ✓ sebagian).
- [ ] **OTA TETAP inline** (revisi R9: deferral ditinggalkan). Cukup pastikan
      flash-erase panjang tak mematikan watchdog cdc_rx; tak perlu jalur deferred.

### Fase 5 — Auth robustness + multi-remote (🟠 S4/F4/F5, F8; F6 catat saja)
- [ ] **S4 (akar flakiness `cp`)**: `waitForAuthorized` benar-benar menunggu resolusi
      auth (retry + timeout lebih panjang), bedakan AUTH_FAIL vs firmware-lama; JANGAN
      lanjut diam-diam `authorized=false`. (Token persist di NVS — bukan masalah.)
- [ ] CLI: bila token ditolak, fallback challenge dengan password tersimpan/prompt
      (`connect --password`), simpan token baru.
- [ ] Forge-web: token per-device (F5); `setRemote` close transport lama (F4);
      reconnect backoff (WF3); tandai transport jaringan "insecure" sampai `wss` (F6).

### Fase 6 — Chunked read + verifikasi dwi-platform & dokumentasi
- [ ] **Chunked read** (device→host streaming, §6.2) — hapus cap 64 KB untuk download
      besar; `copy` streaming src→dst di device (R5).
- [ ] WASM simulator: list/read/write/cli/screen lewat Forge-web hijau, tak
      membekukan page-thread.
- [ ] skyrizz-e32: Forge-CLI & Forge-web; uji reconnect + resume `xfer` (§8.2).
- [ ] Update `docs/feats/remote-protocol.md`, `docs/architecture/`, `STATE.md`.

---

## 13. Matriks keterlacakan cacat → status

Status: ✅ selesai · 🟡 sebagian · ⬜ belum · ➖ ditinggalkan/diubah.

| Cacat | Status | Fase | Catatan |
|---|---|---|---|
| S1/S2/F2 (write 64 KB / RX overflow) | ✅ | 2 | chunked write + `setRxBufferSize(4096)` |
| S3/F1/WF1 (FIFO tanpa reqId) | ✅ | 1 | envelope reqId + `Map` korelasi |
| W9/F10 (RLE/screen tak berbatas) | ✅ | 1 | `rleDecode` bound + validasi `w*h` |
| R2 (reply ke sesi mati) | ✅ | 1 | reqId; **inline → tak lagi lintas-task** |
| R3/R4/R8 (file_rx HOL/drop/race FS) | ✅ | 2 | **dihapus**: inline file-ops (tak ada task/queue) |
| S6 (pre-check SD false-negatif) | ✅ | 2 | jadi peringatan lunak |
| S11 (challenge tak reset) | 🟡 | 2.5 | `#pendingChallenge` direset; `#helloTimer` belum (N4) |
| **N1 (cdc_rx stack inline FS)** | ⬜🔴 | 2.5 | naikkan stack 12 KB + buf off-stack |
| **N2 (xfer_ abaikan reqId → korupsi)** | ⬜🔴 | 2.5 | validasi reqId, tolak clobber |
| **N3 (WriteBegin/End tanpa retry/idemp.)** | ⬜🟠 | 2.5 | retry + WriteEnd idempotent per reqId |
| **N4 (helloTimer bocor / screen nyangkut)** | ⬜🟠 | 2.5 | clear timer + reset `screenWanted_` di drop |
| W1/F11 (frame ceiling) | ➖ | — | marginal (len 16-bit; screen ~64 KB sah) |
| R1/F12 (auth state vs event-thread) | ⬜ | 3 | lock kecil; **R8 sudah hilang via inline** |
| R6 (status 2 buta) | 🟡 | 3 | enum `FileStatus` ada + warn log; errno granular belum |
| R7 (watchdog LittleFS) | ⬜ | 3 | **kini kritis: list inline di cdc_rx** |
| W2 (reset di luar lock) | ⬜ | 3 | untuk MuxTransport |
| R9 (OTA inline blok handshake) | ➖ | 4 | **OTA tetap inline**; deferral ditinggalkan |
| W4/S5 (disconnect/timer) | ⬜ | 4 | heartbeat in-protocol |
| S4 (waitForAuthorized lanjut diam) | ⬜🟠 | 5 | **akar flakiness `cp`** (timing, bukan token) |
| F4/F5/F8/WF3 (web token/slot/parse/reconnect) | ⬜ | 5 | hardening web |
| F6 (`ws://` cleartext) | ➖ | — | Plan 75 (`wss`) |
| R5 (buffer file di RAM) | 🟡 | 6 | write buffer-RAM; read & copy belum streaming |
| readFile cap 64 KB | ⬜ | 6 | chunked read |

---

## 14. Kompatibilitas & migrasi

- **Negosiasi versi**: device dukung HELLO v1 (1 byte) & v2. Forge lama tetap jalan
  (read-only/observation). Forge baru memakai v2 penuh.
- **Roll-out**: Fase 1–2 sudah memperbaiki bug utama tanpa memutus v1 (envelope
  hanya aktif bila kedua sisi v2). Tak ada flag-day.
- **Test bersama** (`klp_test` + TS) menjaga codec & envelope byte-exact lintas
  sisi — kontrak tetap diikat.

---

## 15. Testing

1. **Unit (host)**: codec ceiling, envelope encode/decode, korelasi reqId, resync.
2. **Loopback C++** (`link_test`): bulk-transfer write/read idempotent + resume;
   error codes; heartbeat timeout.
3. **WASM**: Forge-web ↔ simulator — file besar (≥128 KB) lewat bulk-transfer;
   pastikan page-thread tak freeze (chunk + yield).
4. **HW skyrizz-e32**: `cp` `.papp.zip` 10×; reconnect mid-transfer + resume;
   2 remote simultan (USB + BLE) observasi + 1 privileged; OTA tak blok handshake.
5. **Regресi**: `appScan()` setelah `cp` → app muncul di launcher.

---

## 16. Definition of Done

- [ ] Bug `cp /sd/apps/` tuntas di HW + WASM (verifikasi terulang).
- [ ] Semua cacat §0.1 tertutup per matriks §13 (atau dijadwalkan eksplisit bila
      ditunda — mis. `wss` ke Plan 75).
- [ ] `docs/feats/remote-protocol.md` (atau pembaruan) menjelaskan v2 *as-is*.
- [ ] ADR: "reqId correlation + unified bulk-transfer + session model" (Context →
      Decision → Consequences), mereferensi Plan 35/74.
- [ ] `docs/architecture/` lapisan link/remote diperbarui.
- [ ] `STATE.md` baris remote/link → status & tanggal.
- [ ] Commit konvensional per fase (`feat:`/`fix:`/`refactor:`), changelog
      regenerasi otomatis.

---

## 17. Reassessment pasca Fase 1–2 (temuan hardware + review)

> Fase 1–2 selesai & `cp` HW-verified, TAPI proses itu **mengubah beberapa asumsi
> desain awal** dan **memunculkan cacat baru**. Ringkasan agar plan jujur sebelum
> lanjut.

### 17.1 Yang jadi LEBIH MUDAH (diselesaikan "gratis" oleh inline)
Memindah file-ops dari task `file_rx` ke **inline di `cdc_rx`** menghapus seluruh
kelas race lintas-task:
- **R8** (handleFile di `file_rx` vs CLI-FS di `cdc_rx` atas `fs_` yang sama) — hilang.
- **R2** (reply dikirim dari task lain ke koneksi yang sudah mati) — hilang; reply
  inline + reqId.
- **R3/R4** (HOL queue, drop depth-4) — hilang; tak ada queue.
→ Fase 3 mengecil drastis: tinggal **R1** (auth vs event-thread), **W2**, **R6/R7**.

### 17.2 Yang jadi LEBIH SULIT / berubah
- **OTA tetap inline** (revisi R9). Deferral ditinggalkan total karena terbukti
  rapuh; OTA punya timeout panjang sendiri & sesi tunggal, jadi inline diterima.
- **Watchdog `list` (R7) naik jadi kritis**: kini `list` jalan di `cdc_rx`; SD besar
  bisa stall HELLO + trip TWDT. Wajib feed watchdog (SD sudah, LittleFS belum).
- **Stack `cdc_rx`** (N1): task ini sekarang menanggung kedalaman call FAT/LittleFS.
  8 KB + `buf[1024]` on-stack = tipis; **regresi potensial bug stack-overflow lama**.

### 17.3 Cacat BARU dari kode Fase 1–2 (review adversarial) → Fase 2.5
| ID | Sev | Inti |
|---|---|---|
| N1 | 🔴 | `cdc_rx` 8 KB jalankan FS scan inline → stack-overflow non-deterministik |
| N2 | 🔴 | `handleFile` simpan `xfer_.reqId` tapi **tak pernah cek** → 2 client (WS+USB) berbagi 1 `xfer_` → korupsi file diam-diam, dua-duanya di-ack OK |
| N3 | 🟠 | WriteBegin/WriteEnd tanpa retry; WriteEnd tak idempotent (ulang → BadRequest) |
| N4 | 🟠 | `#helloTimer` bocor di `onState(false)` (S5); `screenWanted_` bisa nyangkut ON (USB tak ada disconnect) |
| N5 | 🟡 | Tak ada cek `off===data.length` sebelum WriteEnd (L2); write 0-byte (`data()==nullptr`) belum diverifikasi tiap backend (L3) |

### 17.4 Konfirmasi akar flakiness auth (`cp` kadang gagal)
Token **persist di NVS** (`RemoteAuthStore` → config store; NVS tak ditimpa flash
spiffs), jadi token valid lintas-reboot. Flakiness murni **S4 timing**: saat link
padat, round-trip AUTH > 5 s → `waitForAuthorized` lanjut diam-diam `authorized=false`
→ device drop semua File op. Perbaikan = Fase 5 (klien), bukan persistence.

### 17.5 Urutan kerja yang disarankan
**Fase 2.5 (korektitas) DULU** — N1/N2 (🔴) sebelum apa pun, lalu N3/N4/N5 — karena
keduanya cacat di kode yang BARU di-ship & live di device. Baru lanjut Fase 3 → 5
(auth, agar `cp` mulus tanpa workaround password) → 4 (liveness) → 6 (chunked read).

---

## 18. Hasil eksekusi Fase 2.5–6 (selesai & HW-verified)

Semua fase diimplementasikan, build hijau (host 13 test, ESP32, WASM), dan diuji di
**skyrizz-e32 via Forge CLI**.

### Yang terbukti jalan (HW)
- **`cp` dua-arah byte-identik**: push (chunked write streaming-to-disk) + pull
  (chunked read device→host). File kecil (.papp.zip 4.9 KB) **dan 200 KB** —
  round-trip identik, 5× berturut tanpa reset, device tetap responsif.
- **`palanu exec <dev> <cmd>`** (baru): jalankan satu perintah CLI non-interaktif,
  cetak output, exit (`version`/`ram`/`ps`/`fs ls`…). + `--password` di semua command.
- **Auth**: S4 diperbaiki (tak lanjut diam-diam); fallback password challenge-response.

### Temuan HW penting (mengubah beberapa keputusan)
1. **`xferId` ≠ reqId.** Tiap frame chunked-write pakai reqId baru (korelasi reply);
   identitas transfer dipisah jadi `xferId` (dialokasi device di WriteBegin). N2 fix.
2. **Streaming write ke disk (R5).** Buffer seluruh file di RAM bikin heap fragment →
   wedge. `IFileSystem::writeStream{Begin,Chunk(offset),End,Abort}`; SD/LittleFS tahan
   1 `FILE*` + `fseek` per chunk. MemFS/WASM fallback ke buffered.
3. **🔴 Akar crash file besar = stack overflow `cdc_rx`.** `Guru Meditation
   (InstrFetchProhibited, PC=0x1)` di core GUI: `cdc_rx` 16 KB overrun di jalur dalam
   `handleFile → FATFS → SDSPI → DMA` (lebih dalam saat FATFS flush FAT pada file
   besar) → korup heap tetangga (std::function TaskRunner). **Fix: stack `cdc_rx`
   8→16→32 KB.** Konsekuensi model inline: RX task menanggung kedalaman call FS penuh.
4. **Heartbeat harus longgar.** File I/O inline menahan RX task beberapa detik; timeout
   ketat (6 s) false-positive saat transfer besar. Dilonggarkan ~60 s device / 30 s klien.

### Catatan
- **Kartu SD perlu di-format ulang (FAT32).** Pengujian destruktif (crash-saat-tulis
  SEBELUM fix stack #3) meng-korup FAT kartu uji; `/sd` jadi I/O error, dan karena
  `FileSink` nge-log ke `/sdcard/logs.txt`, SD rusak memperlambat device (auth flaky).
  **Bukan cacat kode** — `/system` (LittleFS) round-trip byte-identik di boot bersih.
- `readFile` & `copy` device-side masih buffer di RAM (chunked-read mengirim per-chunk
  tapi membaca file penuh dulu) — streaming read penuh = follow-up kecil.
- F6 (`wss`) tetap ditunda ke Plan 75.
