# 78 — `@palanu/forge-cli` — Device Remote CLI (USB / Network)

> CLI `palanu` untuk manage multiple devices, remote shell interaktif, dan file copy —
> lewat USB serial atau network (WebSocket). Kayak **SSH ke server Linux**, tapi untuk
> board Palanu. Konsumen pertama `@palanu/link` (plan 77). Independen dari Forge web
> (bisa jadi binary standalone), sesuai filosofi "Forge tanpa server".

- Status: ✅ DONE — `@palanu/forge-cli` scaffolded, 7 commands (add/list/connect/disconnect/remove/shell/cp), Node transports (serial + ws), tests + README. Build green.
- Depends on: **77 (`@palanu/link`)**, 75 (network transport — `WebSocketTransport`
  sudah ada di Forge web), 44/45 (CLI shell + multi-session — sudah done di device side)
- Blocks: — (kaki ekosistem remote CLI standalone)

---

## 0. Konsep

`palanu` = klien remote-only (fokus remote, bukan simulator — itu domain Forge web).
Bicara **PLP** via `@palanu/link` dengan transport Node. Satu binary, manage banyak
device, satu device-CLI yang sama dengan yang dipakai Forge web & on-device.

**Yang SUDAH jalan dan bisa dipakai:**
- PLP CLI channel (0x08) fully functional di device side — 19+ command (`ps`, `hwinfo`,
  `fs ls`, `run`, `ota`, `wlan`, `ble`, `config`, dll).
- Multi-session CLI (plan 45) — per-sid framing, prompt `cwd$`, EOT marker.
- `RemoteSession` (di Forge web, akan diekstrak ke `@palanu/link`) sudah handle semua
  channel: CLI, File, Log, OTA, Screen, Event, System.
- `@palanu/app-sdk` sudah punya `palanu-build` binary untuk build `.papp` — reusable
  untuk `deploy` nanti.

**Yang BELUM ada dan dibuat di plan ini:**
- `packages/forge-cli/` — belum ada scaffolding.
- Node transports: `NodeSerialTransport` (serialport), `NodeWebSocketTransport` (ws).
- CLI binary + arg parser (commander.js).
- Device registry persistent (`~/.palanu/config.json`).
- `FileTokenStore` (implement `ITokenStore` dari plan 77).

---

## 1. Goals — MVP

- [ ] Binary `palanu` (Bun runtime) — transport **USB serial (serialport)** dan
      **network (WebSocket via `ws`)**, keduanya via `ILinkTransport` (plan 77).
- [ ] **BLE transport ditunda** — bukan MVP. `@abandonware/noble` agak rumit
      (platform-specific, pairing). Masuk fase berikutnya.
- [ ] **Manage multiple devices**: `add`/`list`/`connect`/`disconnect`/`remove`.
- [ ] **Remote shell interaktif**: `shell <name>` → REPL dengan prompt `cwd$`, kayak
      SSH ke server. Pakai PLP Cli channel.
- [ ] **File copy**: `cp <src> <dst>` — copy file device↔local, kayak `scp`/`rsync`.
      Pakai PLP File channel.
- [ ] **Device registry**: `~/.palanu/config.json` — alias → target, token per device.

**Non-goal (fase berikutnya, bukan MVP):**
- `deploy` (build `.papp` + push + install) — butuh plan 76 (service) + 38 (persist).
- `ota` firmware update — butuh Ota channel chunk logic (sudah ada di `RemoteSession`,
  tinggal wrap command).
- `service list/start/stop` — butuh plan 76.
- `logs -f` stream — butuh Log channel handler (sudah ada di `RemoteSession`, tinggal
  wrap command).
- `devices` discovery (mDNS + serial enumerate) — nice-to-have, bisa masuk fase 2.
- `auth login/logout` — plan 74 DITUNDA. `RemoteSession` sudah handle `AUTH_*` opcodes;
  kalau device minta auth, CLI bisa prompt password inline tanpa command terpisah.
- Simulator (domain Forge web), GUI, desktop app.

---

## 2. Desain

### 2.1 Struktur

- Package `packages/forge-cli` (`@palanu/forge-cli`, bin: `palanu`). Bun runtime.
- Dependency: `@palanu/link` (protokol), `commander` (arg parser), `serialport`
  (USB serial), `ws` (WebSocket network).
- Transport dipilih dari target URL scheme:
  - `serial:///dev/cu.usbmodem*` → `NodeSerialTransport`
  - `ws://host:port/plp` → `NodeWebSocketTransport`
  - BLE (`ble://MAC`) — ditunda, bukan MVP.

### 2.2 Config & device registry

`~/.palanu/config.json`:

```jsonc
{
  "devices": {
    "mydev": {
      "target": "serial:///dev/cu.usbmodem123",
      "token": "abc123...",       // dari auth handshake (jika device minta)
      "lastConnected": "2026-06-21T12:00:00Z"
    },
    "skyrizz": {
      "target": "ws://skyrizz.local:8477/plp",
      "token": null,
      "lastConnected": null
    }
  }
}
```

- `FileTokenStore` implement `ITokenStore` (dari plan 77) — baca/tulis token per device.
- `add`/`remove` mutate registry. `list` baca registry + cek koneksi aktif.
- `connect`/`disconnect` buka/tutup `RemoteSession` + update status.

### 2.3 Commands (commander.js)

```
palanu add <name> <target>         # daftarkan koneksi (alias → target)
  # contoh: palanu add mydev serial:///dev/cu.usbmodem123
  #         palanu add skyrizz ws://skyrizz.local:8477/plp

palanu list                        # list device: connected ✓ / disconnected ✗
  # output:
  #   NAME       TARGET                              STATUS
  #   mydev      serial:///dev/cu.usbmodem123        ✓ connected
  #   skyrizz    ws://skyrizz.local:8477/plp         ✗ disconnected

palanu connect <name>              # connect ke device by alias
palanu disconnect <name>           # disconnect
palanu remove <name>               # hapus dari registry

palanu shell <name>                # remote CLI interaktif (REPL) — kayak SSH
  # buka RemoteSession, pakai PLP Cli channel (sid=0)
  # tampilkan prompt cwd$ (dari prompt frame device)
  # stream output realtime, arrow up/down history
  # exit / Ctrl-D untuk keluar

palanu cp <src> <dst>              # copy file device↔local — kayak scp
  # format path:
  #   device:mydev:/path/to/file   → remote (di device)
  #   ./local/path                 → local
  # contoh:
  #   palanu cp device:mydev:/system/apps/app.papp ./backup/   # pull
  #   palanu cp ./myapp.papp.zip device:mydev:/system/apps/    # push
  # pakai PLP File channel: readFile() / writeFile()
  # progress bar untuk file besar
```

### 2.4 Command map → PLP channel

| Command | PLP Channel | Catatan |
|---|---|---|
| `shell <name>` | Cli (0x08) | Interactive REPL, `session.sendCli(sid, line)` |
| `cp pull` | File (0x09) | `session.readFile(path)` → write local |
| `cp push` | File (0x09) | read local → `session.writeFile(path, bytes)` |
| `add`/`list`/`remove` | — | Local registry only, no PLP |
| `connect`/`disconnect` | Control (0x00) | Handshake + open/close `RemoteSession` |

### 2.5 UX

- Exit code benar (script-able).
- `--json` output untuk piping (opsional, fase 2).
- Progress bar untuk `cp` file besar.
- `-v` verbose PLP frame log (debug).
- Kalau device minta auth (`AUTH_REQUIRED`), CLI prompt password inline, lakukan
  handshake, simpan token. Tidak butuh command `auth login` terpisah untuk MVP.

---

## 3. Tasks

### 3.1 Scaffold
- [ ] `packages/forge-cli/` — `package.json` (`@palanu/forge-cli`, bin: `palanu`),
      `tsconfig.json`, dependencies (`@palanu/link`, `commander`, `serialport`, `ws`).
- [ ] Entry point `bin/palanu.ts` — setup commander, parse args, dispatch command.
- [ ] `src/registry.ts` — baca/tulis `~/.palanu/config.json` (device registry).

### 3.2 Node transports (implements `ILinkTransport` dari `@palanu/link`)
- [ ] `src/transport/NodeSerialTransport.ts` — pakai `serialport` (USB-CDC).
      Open port, `onData` → frame parser, `send` → write port, `close` → close port.
- [ ] `src/transport/NodeWebSocketTransport.ts` — pakai `ws` (network PLP, plan 75).
      Connect WebSocket, `onData` → frame parser, `send` → ws.send, `close` → ws.close.
- [ ] BLE transport (`NodeBleTransport` via `@abandonware/noble`) — **ditunda**, skip MVP.

### 3.3 Token store
- [ ] `src/FileTokenStore.ts` — implement `ITokenStore` (dari plan 77).
      Baca/tulis token dari `~/.palanu/config.json` per device alias.

### 3.4 Commands
- [ ] `palanu add <name> <target>` — validasi target URL, tulis ke registry.
- [ ] `palanu list` — baca registry, tampilkan tabel (nama/target/status).
      Cek koneksi: cek apakah `RemoteSession` aktif untuk device itu.
- [ ] `palanu connect <name>` — resolve target dari registry, bikin transport +
      `RemoteSession` + `FileTokenStore`, buka koneksi, handshake, simpan status.
- [ ] `palanu disconnect <name>` — tutup `RemoteSession`, update status.
- [ ] `palanu remove <name>` — hapus dari registry.
- [ ] `palanu shell <name>` — interactive REPL:
      - Pastikan connected (auto-connect kalau belum).
      - Pakai PLP Cli channel (0x08), `sid=0`.
      - Tampilkan prompt `cwd$` (dari prompt frame `[0x01]<cwd>`).
      - Baca stdin (readline), `session.sendCli(0, line)`.
      - Stream output: `session.on('cli', chunk => process.stdout.write(chunk.text))`.
      - EOT (`0x04`) → tampilkan prompt baru.
      - `exit` atau Ctrl-D → keluar, tetap connected (user bisa `disconnect` manual).
- [ ] `palanu cp <src> <dst>` — file copy:
      - Parse src/dst: deteksi `device:alias:/path` (remote) vs `./path` (local).
      - Pull: `session.readFile(remotePath)` → write ke local file.
      - Push: read local file → `session.writeFile(remotePath, bytes)`.
      - Progress bar (untuk file > 1KB).

### 3.5 Test & docs
- [ ] Test terhadap **simulator/host** (PLP loopback via `loopbackPair` dari plan 77)
      supaya CI tak butuh hardware.
- [ ] `packages/forge-cli/README.md` — quickstart (add → connect → shell → cp).

---

## 4. Acceptance criteria

- [ ] `palanu add mydev serial:///dev/cu.usbmodem123` → tersimpan di `~/.palanu/config.json`.
- [ ] `palanu add skyrizz ws://skyrizz.local:8477/plp` → tersimpan.
- [ ] `palanu list` menampilkan semua device + status connected ✓ / disconnected ✗.
- [ ] `palanu connect mydev` → buka `RemoteSession` via `NodeSerialTransport`.
- [ ] `palanu connect skyrizz` → buka `RemoteSession` via `NodeWebSocketTransport`.
- [ ] `palanu shell mydev` → REPL interaktif dengan prompt `cwd$`, bisa jalankan
      `hwinfo`, `ps`, `fs ls`, dll. `exit` atau Ctrl-D untuk keluar.
- [ ] `palanu cp device:mydev:/path/to/file ./local/` → file tersalin dari device ke local.
- [ ] `palanu cp ./local/file device:mydev:/path/to/` → file tersalin dari local ke device.
- [ ] `palanu disconnect mydev` → tutup session, status jadi disconnected.
- [ ] `palanu remove mydev` → hapus dari registry.
- [ ] Semua perintah lewat `@palanu/link` — **nol duplikasi** codec/session/transport.
- [ ] Node transports implement `ILinkTransport` — ganti transport = ganti implementasi saja.
- [ ] Kalau device minta auth, CLI prompt password inline, handshake, simpan token di
      `~/.palanu/config.json`. Reconnect berikutnya tak perlu password.

---

## 5. Struktur package target

```
packages/forge-cli/
├── package.json              # @palanu/forge-cli, bin: palanu, type: module
├── tsconfig.json
├── bin/
│   └── palanu.ts             # entry point (commander setup)
├── src/
│   ├── registry.ts           # ~/.palanu/config.json read/write
│   ├── FileTokenStore.ts     # ITokenStore impl (file-backed)
│   ├── transport/
│   │   ├── NodeSerialTransport.ts      # serialport (USB-CDC)
│   │   └── NodeWebSocketTransport.ts   # ws (network PLP)
│   └── commands/
│       ├── add.ts            # palanu add <name> <target>
│       ├── list.ts           # palanu list
│       ├── connect.ts        # palanu connect <name>
│       ├── disconnect.ts     # palanu disconnect <name>
│       ├── remove.ts         # palanu remove <name>
│       ├── shell.ts          # palanu shell <name> (interactive REPL)
│       └── cp.ts             # palanu cp <src> <dst> (file copy)
└── README.md                 # quickstart
```

---

## 6. Fase berikutnya (bukan MVP — setelah 76 + 38 done)

- `palanu deploy <dir>` — build `.papp` via `@palanu/app-sdk` → push via File/Ext
  channel → install. Butuh plan 76 (service) + 38 (persist).
- `palanu service list/start/stop` — kontrol daemon headless. Butuh plan 76.
- `palanu ota <firmware.bin>` — firmware update via Ota channel. Logic sudah ada di
  `RemoteSession.otaUpdate()`, tinggal wrap command.
- `palanu logs -f` — stream Log channel. Handler sudah ada di `RemoteSession`,
  tinggal wrap command.
- `palanu devices` — mDNS discovery (`_palanu._tcp`) + serial port enumeration.
- `palanu auth login/logout/status` — kalau plan 74 sudah done (device-side
  `RemoteAuthStore`). Saat ini CLI prompt password inline sudah cukup.
- BLE transport (`NodeBleTransport` via `@abandonware/noble`).

---

## 7. Dependency chain eksekusi

```
77 (@palanu/link — extract + IDL + refactor Forge web)
  ↓
78 (forge-cli — scaffold + Node transports + commands + test)
```

77 dulu (extract + Forge web refactor), baru 78 (CLI consumer). `deploy`/`ota`/
`service`/`logs`/BLE = fase berikutnya setelah 76 + 38 done.
