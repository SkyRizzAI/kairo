# Link / Remote, Storage & HAL

> The "console substrate": one wire protocol (PLP) lets a host drive the device's screen,
> input, logs, shell and filesystem over any cable; a Linux-style VFS unifies storage
> backends; the HAL abstracts every peripheral behind capability-gated interfaces.

## PLP — Palanu Link Protocol

The wire protocol is **PLP** (Plan 35). The firmware codec is a byte-exact mirror of the
TypeScript codec in `packages/forge/src/lib/plp/codec.ts` — they share test vectors.

**Frame format** (`firmware/core/include/nema/link/plp_codec.h`, encoder in `plp_codec.cpp`):

```
[magic:0xAB][chan:1][flags:1][len:2 LE][payload:len][crc8:1]
```

- `MAGIC = 0xAB`; `HEADER = 5`; CRC covers header+payload.
- **CRC-8/SMBus**, polynomial `0x07`, init `0x00`, MSB-first.
- **Channels**: `Control=0x00, Screen=0x01, Input=0x02, Log=0x03, System=0x04, Ota=0x05,
  Ext=0x06, Event=0x07, Cli=0x08, File=0x09`.
- **Flags**: `None=0, FragMore=1<<0, Compressed=1<<1` (RLE handled at higher layers).
- **FrameParser**: scans for `MAGIC`, waits for full header then full frame, validates CRC,
  emits `Frame{channel,flags,payload}`; **on CRC mismatch advances 1 byte to resync** — so the
  parser tolerates arbitrary chunk boundaries *and* console-log noise interleaved on the wire.
- **RLE** (`rleEncode`/`rleDecode`): `[count][value]` pairs (runs ≤255) to compress the mostly
  blank 1-bit Screen channel.

**Control opcodes** (`link_service.h`): `HELLO=0x01, ACK=0x02, REJECT=0x03, PING=0x10, PONG=0x11`.

**Handshake** (`link_service.cpp`): the **Host** sends `HELLO`; the **Device** sets `ready_`,
replies `ACK`, fires `onReady`. The Host on `ACK` sets `ready_` and fires `onReady`. `PING`→`PONG`
for liveness.

## Transport layer

**`ILinkTransport`** (`link/transport.h`): `send(data,len)`, `onRecv(RecvFn, user)` (raw C
function pointer, not `std::function`), `isConnected()`, `mtu()`. Same file: `LoopbackTransport`
(two ends back-to-back, the virtual-cable model).

| Transport | File | Notes |
|---|---|---|
| `MuxTransport` | `link/mux_transport.h` | Combines up to 4 children (Plan 37). `send` fans out to every **connected** child; `isConnected()` = any child; `mtu()` = **min** child mtu. How USB + BLE coexist on one `LinkService`. |
| `UsbCdcLinkTransport` | `link/usb_cdc_link_transport.h` | Wraps `IUsbCdc` (raw CDC-ACM pipe). mtu 512. |
| `BleLinkTransport` | `link/ble_link_transport.h` (+ `plp_ble.h`) | GATT service `a7b30001-…`, TX notify `a7b30002-…`, RX write `a7b30003-…`. mtu 180. UUIDs must match `packages/forge/src/lib/plp/uuids.ts`. |
| `WasmCableTransport` | `firmware/platforms/wasm/src/wasm_cable_transport.cpp` | Simulator cable; marshals bytes to JS over the shared WASM heap. |

**`LinkService`** (`link/link_service.*`) owns the `FrameParser` and runs the session over one
transport. `attach(t, role)` resets state and registers the recv callback. Control frames are
always processed; **app channels are gated — delivered only when `ready_`**. `send()` drops
non-Control frames before handshake and **serializes all transmits under `sendMtx_`** (the GUI
thread, RX task, and app threads all send concurrently; without the lock, MTU-sized writes
interleave into corrupt frames). `markDisconnected()` is idempotent.

Platform wiring (`esp32_platform.cpp`): USB-CDC added unconditionally; BLE added if `caps::BtBle`;
both into `mux_`, then `link_.attach(&mux_, Role::Device)`. The PLP substrate comes up
independent of any display — a headless board is fully usable over CLI.

> See [ADR 0001](../decisions/0001-usb-jtag-remote-uses-hwcdc.md): in USB JTAG/Serial mode the
> `IUsbCdc` impl must drive HWCDC (native USB Serial/JTAG), not Arduino `Serial` (= UART0).

## RemoteService & CLI

**`RemoteService`** (`services/remote_service.*`) is the device-side orchestrator. `init()` wires
`onFrame`→`dispatch`. Attach points: `attachLog/attachEvents/attachCli/attachSessions/attachFs/
attachOta/onPower/onControl/setProfile`.

Channel dispatch:
- **Input** (0x02): `input_->post((Key)payload[0])`.
- **System** (0x04): `GetInfo(0x01)` → board-profile JSON; opcodes ≥ `Restart(0x10)` → platform `powerFn`.
- **Cli** (0x08): `[sid][line]` → `sessions_->get(sid)`, run `cli_->execute(line, s)`, send a
  prompt update + EOT `[sid][0x04]`.
- **File** (0x09): `handleFile()` (below).
- **Ota** (0x05): `handleOta()` (below).
- **Ext** (0x06): `InjectEvent(0x01)` publishes onto the EventBus; else (e.g. `WifiSetNetworks`,
  `AppInstall` of a raw `.papp`) → board `controlFn`.
- **Log/Event** (outbound only): `attachEvents` subscribes `"*"` and serializes events on the
  Event channel; `LinkLogSink` serializes log entries on the Log channel — **both gated on
  `link->ready()`**.

**OTA flow** (`handleOta`): `Begin(0x01)` carries `[size:4]`, opens the inactive slot, replies
with `protoVersion=2` (so the host detects stale firmware). `Data(0x02)` `[offset:4][bytes]` is
**idempotent by offset** (off==written → write; off<written → ack; off>written → error so the host
resyncs). `End(0x03)` commits and may `SysOp::Restart`. `Abort(0x04)` discards. Backed by
`IOtaUpdater` (`hal/ota.h`); ESP32 `esp32_ota.cpp` uses `esp_ota_*` + rollback `confirmBoot()`;
WASM is a dry-run (`rebootOnCommit()=false`). Only the PLP push path exists today.

**File flow** (`handleFile`): replies `[op][status][path\0][extra]` (0=ok,1=not found,2=error);
`List` returns `[type][size:4][name\0]…`; all ops delegate to the attached `IFileSystem` (the VFS).

**CLI** (`services/cli_service.*`) is a command **registry** (not a Unix shell). `add(name,help,
handler)`; `execute(line, session)` dispatches with a `CliContext{args,out,session}`. `CliSession`
holds per-connection state (cwd over the VFS, history, output sink, `PATH` scanned for app
auto-launch — Plan 57). `CliSessionManager` owns sessions keyed by 1-byte id (stable storage so
`CliSession&` stays valid); cleared on disconnect. `registerCoreCliCommands(cli, rt)` installs
built-ins (help, hwinfo, ram, caps, display, power, wlan, ble, whoami, profile, fs, pwd/cd,
history); platforms specialize (ESP32 replaces `ram` with live heap).

## Storage / VFS

**`IFileSystem`** (`hal/filesystem.h`, an `IDriver` with `kind()==Storage`): `list/read/write/
mkdir/remove/rename`. v1 is **whole-file** I/O (no offset/chunked yet); paths absolute,
`/`-separated. `FsEntry{name,isDir,size}`.

**`Vfs`** (`fs/vfs.*`) is itself an `IFileSystem` composite — a single namespace rooted at `/`.
`mount(point, fs)` / `unmount`. The mount list is kept **longest-point-first** so `resolve()`
picks the most specific backend and strips the prefix (`/sd/log.txt` → sdcard `/log.txt`).
`list()` **synthesizes** a dir entry for each mount point that is a direct child of the listed dir
(so `ls /` shows `sd`/`tmp`). You cannot `mkdir/remove/rename` onto/across mount points.

**Backends**: `MemFileSystem` (`fs/mem_filesystem.*`, in-RAM, volatile, `seed()`); `LittleFsFileSystem`
(`platforms/esp32/src/littlefs_filesystem.cpp`, persistent internal flash); `SdFatFileSystem`
(`platforms/esp32/src/sd_fat_filesystem.cpp`, FAT microSD).

**Mount tables**: ESP32 → `/` LittleFS (persistent, seeds `/apps`,`/data`,`/badusb` + factory
scripts), `/tmp` Mem (volatile, mounts even if root fails), `/sd` SdFat (only if a card is present).
WASM → `/` Mem + `/sd` Mem (2nd-partition demo). The VFS is handed to `RemoteService::attachFs`, so
the PLP File channel and the Forge browser see one unified tree.

## HAL interfaces

All in `firmware/core/include/nema/hal/`; each derives `IDriver` (`driver.h`: `name()/kind()/
onRegister(rt)`; `DriverKind` = Battery/Wifi/Bluetooth/Display/Storage/Other). Platforms are
**esp32** and **wasm** (there is no native simulator — it is WASM).

| Interface | Header | ESP32 | WASM |
|---|---|---|---|
| `IDisplayDriver` (1-bit, blitRgb565, sleep/wake, dpi) | `display.h` | board LCD/e-ink | `NullDisplay` + RemoteScreenTap (streamed) |
| `IBatteryDriver` | `battery.h` | board / `DummyBatteryDriver` | `DummyBatteryDriver` |
| `IWifiDriver` (blocking, on worker) | `wifi.h` | `esp32_wifi_driver.cpp` | `sim_wifi_driver.cpp` (virtual router) |
| `IBluetoothController` + `IBleAdapter` (+ `IClassicAdapter`, no impl) | `bluetooth.h` | `esp32_ble.cpp` (NimBLE) | — |
| `IHttpClient` (blocking HTTPS) | `http_client.h` | `esp32_http_client.cpp` | — |
| `IUsbCdc` (raw CDC pipe) | `usb_cdc.h` | `esp32_usb_cdc.cpp` | uses WasmCableTransport instead |
| `IUsbHid` (BadUSB keystrokes) | `usb_hid.h` | `esp32_usb_hid.cpp` | — |
| `IFileSystem` | `filesystem.h` | LittleFS / SdFat | MemFileSystem |
| `IOtaUpdater` | `ota.h` | `esp32_ota.cpp` | dry-run |
| `IAsyncDisplay`/`BufferDisplay`, `IAudioInput/Output`, `ICamera`, `IRemoteScreenTap` | resp. headers | board-specific | RemoteScreenTap (core) |

Drivers register via `rt.container().registerAs<I…>()` and declare capabilities; app/core code
branches on `rt.capabilities().has("wifi")` etc., never on board type.

## Conventions & gotchas

- **App channels are gated until the handshake completes** — `send` drops non-Control frames
  before `ready_`; inbound app frames are delivered only after; Log/Event sinks self-check too.
- **`send()` is thread-safe; the rest is not** — `sendMtx_` serializes transmits to avoid
  interleaving. All transport callbacks are raw C function pointers.
- **`markDisconnected` fires on the transport/event thread** — handlers must be thread-safe and
  must NOT call `CapabilityRegistry::setState` directly (route through the async event path).
- **MuxTransport mtu = min of children**; `send` reaches connected children only — higher layers
  must respect `mtu()` (BLE is only 180).
- **OTA Data is idempotent by offset**; the Begin reply carries `protoVersion=2` to surface
  stale-firmware mismatches.
- **VFS**: longest-match routing; synthetic mount-point dirs; no cross-backend rename.
- **PLP UUIDs + codec are a cross-repo contract** — keep `plp_ble.h`/`plp_codec.h` byte-identical
  to `packages/forge/src/lib/plp/{uuids,codec}.ts`.
- **The PLP substrate is display-independent** (Plan 42); `/tmp` (RAM) mounts even if the root FS
  fails, so storage stays usable.

## Key files

| Area | File |
|---|---|
| PLP codec / parser / CRC / RLE | `firmware/core/include/nema/link/plp_codec.h` + `src/link/plp_codec.cpp` |
| Transports | `firmware/core/include/nema/link/{transport,mux_transport,usb_cdc_link_transport,ble_link_transport,plp_ble}.h`, `platforms/wasm/src/wasm_cable_transport.cpp` |
| LinkService | `firmware/core/include/nema/link/link_service.h` + `src/link/link_service.cpp` |
| RemoteService / CLI | `firmware/core/{include/nema,src}/services/{remote_service,cli_service}.*` |
| VFS / backends | `firmware/core/{include/nema,src}/fs/{vfs,mem_filesystem}.*`, `platforms/esp32/src/{littlefs,sd_fat}_filesystem.cpp` |
| HAL | `firmware/core/include/nema/hal/*.h`, `platforms/esp32/src/esp32_ota.cpp` |
