# 35 — Nema Link Protocol & Remote Layer (device-side)

> **LAYER 2 dari 3** (Foundation (34) → **Remote Layer** → Forge (36)). Satu
> protokol, banyak transport. **Nema Link Protocol (KLP)** — framing + channel
> multiplexing + handshake aman — berjalan di atas `ILinkTransport`, yang
> membungkus pipa byte dari Layer 1: **BLE**, **USB-CDC**, atau **virtual-cable**
> (WASM, postMessage). Di atasnya, **RemoteService** men-stream framebuffer 1-bit
> (RLE) + log + telemetry, dan menyuntik input/command — semuanya **device-side**,
> transport-agnostic. Plan ini TIDAK menyentuh web; klien (Forge) ada di Plan 36.

- Status: ✅ Device-side selesai (host + WASM + ESP32-S3). KLP codec C++ (`core/link/klp_codec`, mirror codec.ts) + `ILinkTransport`/Loopback + `LinkService` (handshake+channel gate) + `RemoteScreenTap` (IDisplayDriver decorator, RLE screen stream) + `RemoteService` (input/log/event/system/ext dispatch). Tests `klp_test` + `link_test` PASS. **Transport lengkap: `WasmCableTransport` (WASM virtual-cable, terverifikasi di Chrome), `BleLinkTransport` (`core/link/ble_link_transport.h`, bungkus `IBleAdapter` + KLP GATT service di `Esp32Ble`), `UsbCdcLinkTransport` (`core/link/usb_cdc_link_transport.h`, bungkus `IUsbCdc`).** `Esp32Platform::postRegister` mewire RemoteService over BLE (screen-tap men-decorate display board) — `idf.py build` hijau. Verifikasi BLE fisik (HP/Forge Web Bluetooth) menunggu device terhubung. USB device-side esp32 deferred (lihat Plan 34 §6 — port USB dipakai console). **(+Plan 37, aditif)** Channel `Ext` punya opcode baru `ExtOp::AppInstall = 0x03` (host kirim `.kapp` mentah → device `JsAppStore::installKapp`), di-handle lewat `controlFn_` platform (wasm/esp32) — dispatch inti tidak berubah.
- Milestone: M9 (Board Profile & Ecosystem Foundation)
- Depends on: **34 (Connectivity Foundation: BLE + USB)**, 13 (Display HAL 1-bit + Canvas), 19.5 (Nema/TaskRunner)
- Blocks: **36 (Palanu Forge — web client)**, OTA

> Kontrak kejujuran: KLP codec ada di DUA sisi (C++ device + TS Forge) dan itu
> kontraknya, diikat **test vector bersama**. Remote layer bisa diuji penuh lewat
> **virtual-cable (loopback/WASM) TANPA hardware** dulu, baru BLE/USB.

---

## 0. Posisi dalam layering & insight

Layer 1 (Plan 34) memberi **pipa byte mentah**: BLE (GATT notify/write), USB-CDC,
dan — untuk simulator — tak ada (itu disuplai virtual-cable di sini). Layer 2 ini
menambah **struktur**: framing, channel, handshake, lalu service yang memakainya.

> **Insight kunci (dari diskusi):** protokol remote SELALU sama (KLP). Yang dibuat
> khusus per koneksi cuma **transport/bridge**-nya. "Virtual cable" = transport
> untuk simulator (postMessage ke WASM), **sejajar** BLE & USB. Maka "remote bisa
> me-remote simulator" jadi otomatis: pilih transport `virtual-cable` alih-alih
> `ble`. Konsumen (Forge, Plan 36) tak peduli mana.

```
        RemoteService (device-side, transport-agnostic)
        screen-tap · input-inject · log-sink · system
                         │  KLP codec (C++)
                         ▼
                  ILinkTransport
        ┌──────────────┼───────────────┬───────────────┐
   BleLinkTransport  UsbCdcTransport  WasmCableTransport
   (wraps 34 BLE)    (wraps 34 USB)   (postMessage, WASM)
        │               │                │
   ── pipa byte Layer 1 (Plan 34) ──     └─ ke Worker host (Forge, Plan 36)
```

---

## 1. Goal

1. **KLP codec (C++)** — frame, channel mux, RLE, crc8, handshake. Mirror byte-exact
   dari codec TS (`packages/forge/src/lib/klp/`, sudah ada) + **test vector bersama**.
2. **ILinkTransport** (device) + 3 impl: `BleLinkTransport` (Plan 34 BLE),
   `UsbCdcTransport` (Plan 34 USB), `WasmCableTransport` (postMessage).
3. **RemoteService** — `RemoteScreenTap` (stream layar 1-bit RLE), `RemoteInputSource`
   (inject ke InputService), `LinkLogSink` (stream log), system channel (device info
   + power). Gating handshake.
4. **WASM firmware target** (`firmware/platforms/wasm/` + target) — firmware ASLI
   (core+apps+screens) jalan di browser via Emscripten, bicara KLP lewat
   `WasmCableTransport`. Inilah "simulator di WASM" sebagai **device KLP penuh**.
5. Semua teruji di **virtual-cable/loopback** tanpa hardware; BLE/USB menyusul.

---

## 2. KLP — wire format (kontrak, sudah ada di TS)

```
[magic:0xAB][chan:1][flags:1][len:2 LE][payload:len][crc8:1]
flags bit0 = FRAG_MORE   bit1 = COMPRESSED(RLE)
```
Channel: `0x00` CONTROL (HELLO/ACK/REJECT/PING) · `0x01` SCREEN (1-bit RLE) ·
`0x02` INPUT (action/pointer) · `0x03` LOG · `0x04` SYSTEM (info + power) ·
`0x05` OTA · `0x06` EXT (WASM-only: time/step/mem).

**Handshake (gate semua channel sampai ACK):**
```
host → CONTROL HELLO { proto_ver, token(16B) }
dev  → CONTROL ACK   { proto_ver, caps[] }   |   REJECT { reason }
```
`token` di-derive dari shared secret pairing BLE (Plan 34) / device token NVS.
Ini **lapis kedua** di atas LE Secure — bisa di-revoke tanpa unpair, dukung
multi-device. Untuk virtual-cable & USB (pipa lokal/fisik) token boleh kosong/dev.

> Codec TS sudah ada & teruji (`codec.ts`, 7/7 test: frame, fragmen, CRC resync via
> magic byte, RLE, loopback). Codec C++ (`core/link/klp_codec.{h,cpp}`) meniru ini;
> `firmware/tests/klp_test.cpp` memakai **test vector yang sama** → jaminan sepadan.

---

## 3. ILinkTransport (`core/link/transport.h`)

```cpp
struct ILinkTransport {
    virtual ~ILinkTransport() = default;
    virtual bool   send(const uint8_t* data, size_t len) = 0;  // satu datagram (≤ mtu)
    using RecvFn = void(*)(void* user, const uint8_t* data, size_t len);
    virtual void   onRecv(RecvFn fn, void* user) = 0;
    virtual bool   isConnected() const = 0;
    virtual size_t mtu() const = 0;
};
```
Impl (membungkus Layer 1):
- `BleLinkTransport` — `send`=`IBleAdapter::notify(KLP_CHAR)`, `onWrite`→`RecvFn`. (Plan 34 BLE)
- `UsbCdcTransport` — `send`=`IUsbCdc::write`, `onData`→`RecvFn`. (Plan 34 USB)
- `WasmCableTransport` — `send`=`EM_JS` postMessage; RX = fungsi C ter-export dari worker host.

---

## 4. RemoteService (device-side)

- **`RemoteScreenTap`** (`core/hal/remote_screen_tap.{h,cpp}`) — decorator
  `IDisplayDriver` di rantai `Canvas → AsyncDisplayDriver → RemoteScreenTap → LcdDriver`.
  Teruskan semua ke inner (kaca normal); pada `flush()` bila ada session → RLE 1-bit
  → SCREEN channel. Tanpa session: overhead ≈ nol. **Tap sebelum konversi RGB565**
  → tak ada dekompresi warna, board/Canvas tak berubah.
- **`RemoteInputSource`** (`core/link/remote_input.{h,cpp}`) — INPUT channel →
  `rt.input().post(Code/Action)` / `postPointer()`. Suntik transparan ke funnel
  InputService (identik IKeyMap/ITouchDriver).
- **`LinkLogSink`** (`core/log/link_log_sink.{h,cpp}`) — sink Logger tambahan →
  LOG channel (live log di klien).
- **SYSTEM channel** — device info (`rt.info()` + BoardProfile JSON Plan 33) + power
  (`requestRestart`/`dpm().sleep`/`requestShutdown`).
- **`RemoteService`** (`core/services/remote_service.{h,cpp}`) — `LinkService` (KLP
  codec + channel dispatch + handshake gate) + register semua handler. Aktif bila
  ada transport (capability `bluetooth.ble` / `usb` / build WASM).

---

## 5. WASM firmware target (`firmware/platforms/wasm/`)

Firmware ASLI jalan di browser (Web Worker), bicara KLP via `WasmCableTransport`.
Inilah "simulator di WASM" = **device KLP penuh** (bukan protokol JSON-lines lama).

```
firmware/platforms/wasm/
├─ wasm_platform.{h,cpp}      # IPlatform: clock(emscripten), config(localStorage via JS), idle
├─ wasm_cable_transport.cpp   # postMessage ↔ KLP datagram
└─ wasm_main.cpp              # emscripten_set_main_loop(rt.step)
firmware/targets/wasm/        # rakit: WasmPlatform + board simulator + RemoteService
```
- Board = reuse `simulator` (virtual). Display device = `RemoteScreenTap` (stream),
  input dari `RemoteInputSource`. **Tak perlu** display/input WASM khusus — jalurnya
  = jalur remote.
- EXT channel: extra WASM-only (pause/step/time-scale). Device fisik abaikan.
- Build: Emscripten (`emcmake`) → `nema.wasm` + glue → di-copy ke
  `packages/forge/static/` (dipakai Plan 36). **Prasyarat: emsdk terinstal.**

> Inilah jembatan ke "remote me-remote simulator": WASM = device yang expose KLP
> lewat virtual-cable. Forge (Plan 36) bisa attach `/simulator` DAN `/remote` ke
> instance WASM yang sama — keduanya RemoteSession, beda transport.
>
> **Interface USB/BLE VIRTUAL (target, lihat Plan 36 §0a):** simulator-WASM punya
> HAL yang sama dengan device asli (`IUsbCdc`, `IBleAdapter`). Di WASM, interface
> itu di-back implementasi **virtual in-process**, sehingga remote menyambung ke
> simulator **persis seperti ke hardware** (lewat "USB"/"BLE"), bukan jalur
> khusus. `WasmCableTransport` saat ini berperan sebagai interface virtual itu;
> penajaman agar memetakan langsung ke `IUsbCdc`/`IBleAdapter` virtual = lanjutan.

---

## 6. File Plan (device-side)

| File | Aksi |
|---|---|
| `core/link/klp_codec.{h,cpp}` , `transport.h` | **baru** — KLP codec C++ + ILinkTransport |
| `core/services/link_service.{h,cpp}` | **baru** — codec + channel dispatch + handshake |
| `core/services/remote_service.{h,cpp}` | **baru** — orchestrator |
| `core/hal/remote_screen_tap.{h,cpp}` | **baru** — decorator IDisplayDriver |
| `core/link/remote_input.{h,cpp}` | **baru** — INPUT → InputService |
| `core/log/link_log_sink.{h,cpp}` | **baru** — log → LOG channel |
| `core/link/ble_link_transport.h` | ✅ **baru** — bungkus IBleAdapter (34), header-only, board-agnostic |
| `core/link/usb_cdc_link_transport.h` | ✅ **baru** — bungkus IUsbCdc (34), header-only |
| `core/link/klp_ble.h` | ✅ **baru** — UUID GATT KLP (kontrak dgn Forge `klp/uuids.ts`) |
| `platforms/esp32/.../esp32_ble.{h,cpp}` | ✅ **baru** — NimBLE radio + GATT KLP service |
| `platforms/esp32/.../esp32_platform.cpp` | ✅ `postRegister` mewire RemoteService over BLE |
| `platforms/wasm/**` , `targets/wasm/**` | **baru** — WASM platform + target + virtual cable |
| `boards/skyrizz-e32/.../skyrizz_e32.cpp` | sisip `RemoteScreenTap` di rantai display |
| `firmware/tests/klp_test.cpp` | **baru** — test vector (sepadan dgn codec TS Forge) |
| `firmware/tools/build-wasm.sh` , root `build:wasm` | **baru** |

---

## 7. Fase Implementasi

1. **KLP codec C++ + test vector** — mirror `codec.ts`; `klp_test.cpp` sepadan
   byte-exact (frame/fragmen/CRC/RLE/handshake). Host build, no hardware.
2. **ILinkTransport + LinkService + loopback** — `RemoteService` di atas loopback
   transport (host). Verifikasi handshake + channel dispatch. No hardware.
3. **RemoteScreenTap + RemoteInputSource + LinkLogSink** — di host/sim: frame
   ter-encode, input ter-inject, log ter-stream lewat loopback.
4. **WASM target + WasmCableTransport** — firmware-wasm + virtual cable. *(emsdk)*.
   Output `nema.wasm`. Siap dikonsumsi Forge (Plan 36).
5. **BleLinkTransport** — bungkus IBleAdapter (34). Flash + tes KLP lewat BLE nyata.
6. **UsbCdcTransport** — bungkus IUsbCdc (34). Tes lewat USB. *(opsional bila CDC siap)*.

Fase 1–4 tanpa hardware (loopback + WASM); 5–6 dengan hardware.

---

## 8. Acceptance criteria

- [ ] Test vector C++ ↔ TS sepadan (frame/CRC/RLE/handshake)
- [ ] Frame > MTU ter-fragment & reassembly benar; CRC korup → resync (magic byte)
- [ ] Channel SCREEN/INPUT/OTA di-gate sampai HELLO/ACK
- [ ] `RemoteScreenTap` overhead ≈ nol tanpa session (fps device tak turun)
- [ ] `RemoteInputSource` menyuntik ke InputService yang sama (sama seperti tombol)
- [ ] WASM: firmware asli jalan di Worker, expose KLP via virtual-cable (frame keluar, input masuk)
- [ ] BLE: setelah pair+handshake (34), KLP mengalir; LE Secure + token aktif
- [ ] Core & board tak berubah selain sisip `RemoteScreenTap`

---

## 9. Non-Goals (v1)
- **Web client / UI** (RemoteSession, /remote, /flash) — Plan 36.
- Multi-client simultan pada satu device — v1 satu session (LinkHub fan-out = Forge, Plan 36).
- Video/grayscale stream — v1 1-bit (UI Palanu mono).
- Semantic mirroring — v1 pixel streaming (app-agnostic).
- OTA partition logic mendalam — channel didefinisikan; tulis-partisi menyusul.

---

## 10. Hubungan dengan plan lain
- **34** menyediakan pipa byte (BLE/USB) yang dibungkus `ILinkTransport` di sini.
- **36** mengonsumsi: KLP codec TS (sudah ada di Forge), `nema.wasm`, dan
  meng-attach RemoteSession ke transport (Web Bluetooth/Web Serial/virtual-cable).
- **13** = sumber framebuffer 1-bit (di-tap sebelum RGB565).
- **33** = BoardProfile JSON (dikirim via SYSTEM channel → render device di Forge).
- **27/29** — `RemoteInputSource` = funnel InputService yang sama; pointer = `postPointer`.
