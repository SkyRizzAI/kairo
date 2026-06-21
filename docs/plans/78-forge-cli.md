# 78 — Forge CLI (`palanu`) — Remote Device CLI over USB / BLE / Network

> CLI ala **doctl/aws/gcp** untuk Palanu: akses CLI hardware dari jarak jauh, stream logs,
> transfer file, **deploy/upload apps**, dan **OTA firmware** — lewat USB, BLE, atau jaringan
> (TCP). Konsumen pertama `@palanu/link` (plan 77). Independen dari Forge web (bisa jadi
> binary standalone), sesuai filosofi "Forge tanpa server".

- Status: 🔴 Not started
- Depends on: **77 (`@palanu/link`)**, **75 (TCP transport)**, **74 (auth)**, 38 (persist — deploy), 76 (service — deploy `--service`)
- Blocks: —  (kaki ekosistem terakhir untuk MVP network)

---

## 0. Konsep

`palanu` = klien remote-only (fokus remote, bukan simulator — itu domain Forge web). Bicara
**PLP** via `@palanu/link` dengan transport Node. Satu binary, banyak transport, satu
device-CLI yang sama dengan yang dipakai Forge web & on-device. Login berbasis **session
token** (plan 74) — sekali `auth login`, perintah berikutnya tak perlu password.

```
$ palanu devices                 # discover (mDNS _palanu._tcp + serial ports)
$ palanu auth login skyrizz.local
  Password: ****                  # → token tersimpan di ~/.palanu/config.json
$ palanu logs -f                  # stream Log channel
$ palanu cli hwinfo               # one-shot device CLI (PLP Cli channel)
$ palanu deploy ./my-app --service --autostart
$ palanu ota firmware.bin
```

---

## 1. Goals

- [ ] Binary `palanu` (Bun/Node) — transport **USB (serialport)**, **BLE (noble)**, **TCP
      (net.Socket + mDNS)**, semua via `ITransport` (plan 77).
- [ ] **Auth/login** (plan 74): `auth login/logout/status`, token di `~/.palanu/config.json`.
- [ ] **Device CLI**: `cli [cmd]` (one-shot & interaktif) → PLP Cli channel.
- [ ] **Logs**: `logs [-f]` → stream Log channel.
- [ ] **Deploy apps**: `deploy <dir|.kapp> [--service] [--autostart]` → build `.kapp` (via
      `@palanu/app-sdk`) → push (File/Ext channel) → install (persist plan 38).
- [ ] **Service control**: `service list/start/stop/status` (lewat Cli channel, plan 76).
- [ ] **File transfer**: `fs ls/cp/rm` (File channel).
- [ ] **OTA**: `ota <firmware.bin>` (Ota channel, idempotent chunk — reuse logika existing).
- [ ] **Discovery**: `devices` (mDNS + serial enumerate).

**Non-goal:** simulator (Forge web), GUI, desktop app (Tauri/Electron — future, tapi karena
CLI pakai `@palanu/link` yang sama, jalannya terbuka).

---

## 2. Desain

### 2.1 Struktur
- Package `packages/forge-cli` (`@palanu/cli`, bin: `palanu`). Bun runtime.
- Dependency: `@palanu/link` (protokol), `@palanu/app-sdk` (build `.kapp`), `serialport`,
  `@abandonware/noble` (BLE), `ws` (WebSocket network — sama dgn Forge web), `bonjour`/`mdns`
  (discovery).
- Transport dipilih otomatis dari target arg (`usbserial://`, `ble://MAC`, `ws://host:port/plp`,
  `tcp://host:port`, atau nama mDNS) — atau `--transport`. **Network default = WebSocket**
  (jalur yang sama dengan Forge web, plan 75); raw TCP opsional.

### 2.2 Config & auth
- `~/.palanu/config.json`: daftar device (alias→target), token per device (`ITokenStore` impl
  dari plan 77). `auth login` lakukan handshake (plan 74) → simpan token; perintah lain
  load token; `403/AuthRequired` → minta `auth login` ulang.

### 2.3 Command map → PLP channel
| Command | Channel | Catatan |
|---|---|---|
| `cli [cmd]` | Cli | one-shot (exit code) atau REPL (`-i`) |
| `logs [-f]` | Log | follow = stream sampai Ctrl-C |
| `deploy` | File + Ext (AppInstall) | build→upload→install; `--service`→type:service (76) |
| `service …` | Cli (`svc …`) | kontrol daemon (plan 76) |
| `fs ls/cp/rm` | File | List/Read/Write/Remove |
| `ota` | Ota | chunk offset-based idempotent (reuse) |
| `info` | System (GetInfo) | board/fw/caps |
| `devices` | — | mDNS + serial enumerate (pra-koneksi) |

### 2.4 UX (rasa doctl)
- Exit code benar (script-able), `--json` output untuk piping, progress bar untuk
  deploy/ota, `-v` verbose PLP frame log.
- Semua perintah privileged butuh login (plan 74); observasi (`logs`,`info`) jalan tanpa auth
  kalau device mengizinkan tier observation.

## 3. Tasks
- [ ] Scaffold `packages/forge-cli` + bin `palanu` + arg parser.
- [ ] Transport Node: `SerialPortTransport`, `NobleBleTransport`, `TcpTransport`(+mDNS) — impl `ITransport`.
- [ ] `auth login/logout/status` + `~/.palanu/config.json` token store.
- [ ] `cli`, `logs`, `info`, `devices`.
- [ ] `deploy` (build via app-sdk → upload → install) + `--service/--autostart` (plan 76).
- [ ] `fs` + `ota` + `service`.
- [ ] Dok `packages/forge-cli/README.md` (quickstart ala doctl) + contoh.
- [ ] Tes terhadap **simulator/host** (PLP loopback) supaya CI tak butuh hardware.

## 4. Acceptance criteria
- [ ] `palanu devices` menemukan device via mDNS (TCP) **dan** serial (USB).
- [ ] `auth login` simpan token; reconnect berikutnya tanpa password; `logout`/revoke memutus.
- [ ] `palanu cli hwinfo` mengembalikan output device CLI yang sama dengan Forge web/on-device.
- [ ] `palanu deploy ./app --service --autostart` → app jalan headless & tahan reboot (76+38).
- [ ] `palanu logs -f` stream realtime; `palanu ota fw.bin` sukses + resumable.
- [ ] Privileged command ditolak tanpa login (plan 74); observasi jalan sesuai kebijakan device.
- [ ] **Nol duplikasi protokol**: semua lewat `@palanu/link` (plan 77), bukan codec lokal.
