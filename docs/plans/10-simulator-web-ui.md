# 10 — Simulator Web UI (Bun relay + React panels)

> Sisi web: `packages/simulator` jadi (a) **Bun server** yang spawn `kairo-sim` & relay stdio↔WebSocket, dan (b) **React UI** dengan 4 panel: **Logs, Events, Services, Controls**.

- Status: ☐ Not started
- Milestone: M2 (Observability) + M3 (Simulator)
- Depends on: 09 (protokol & binary JSON-lines siap)
- Blocks: 11

---

## Goal

- Server Bun (`Bun.serve`) yang:
  - Serve React UI (HTML import, **tanpa Vite**).
  - Buka endpoint **WebSocket**.
  - Saat command `boot` → **spawn** `kairo-sim` (`Bun.spawn`, env `KAIRO_SIM_JSON=1`), pipe **stdout** → parse JSON-lines → broadcast ke WS; pipe WS command → **stdin** binary.
  - `shutdown`/`restart` → kelola lifecycle proses (kill / re-spawn; hormati exit code restart dari stage 09).
- React UI dengan 4 panel + indikator status koneksi/boot.

## Scope

### In scope

- `packages/simulator/index.ts` (server + relay + spawn manager).
- `packages/simulator/index.html` + `frontend.tsx` (React root).
- Komponen panel: `LogsPanel`, `EventsPanel`, `ServicesPanel`, `ControlsPanel`.
- State store sederhana (React state/context) yang mengkonsumsi stream WS.
- Filter level di LogsPanel (TRACE..FATAL).
- `package.json` script `dev`/`start`.

### Out of scope

- Panel Display / Hardware Registry / Capability Registry (di luar 4 panel MVP — boleh ditambah belakangan; data sudah ada dari stage 08/09).
- Auth, multi-session, persistence.
- Styling mewah (cukup fungsional & rapi; boleh Tailwind via Bun bila mudah).

---

## Design

### Arsitektur server (Bun)

```text
Browser ──WS──► Bun.serve ──┬─ spawn kairo-sim (KAIRO_SIM_JSON=1)
                            │   ├─ proc.stdout → readline → JSON → ws.publish(topic)
                            │   └─ ws.message(cmd) → proc.stdin.write(JSON+"\n")
                            └─ serve index.html (React)
```

- Gunakan **topic pub/sub** `Bun.serve` (`ws.subscribe("sim"); server.publish("sim", msg)`) agar broadcast ke semua client mudah.
- **Spawn manager** (modul kecil): `boot()`, `shutdown()`, `restart()`, status. Menyimpan handle `Bun.spawn`. Baca stdout via `for await (const chunk of proc.stdout)` + buffer pemecah baris (`\n`). Tangani exit: kalau exit code = kode `restart` (stage 09, mis. 75) → auto re-spawn; selain itu broadcast `{"type":"sim_exit","code":...}`.
- Lokasi binary: cari di `firmware/build/targets/simulator/kairo-sim` relatif root repo. Kalau belum ada → broadcast pesan error "build dulu" (jangan crash server).

### Server sketsa (`index.ts`)

```ts
import index from "./index.html";

let proc: Bun.Subprocess | null = null;
let lineBuf = "";

function broadcast(server: Bun.Server, obj: unknown) {
  server.publish("sim", JSON.stringify(obj));
}

async function pumpStdout(server: Bun.Server) {
  if (!proc?.stdout) return;
  for await (const chunk of proc.stdout as ReadableStream<Uint8Array>) {
    lineBuf += new TextDecoder().decode(chunk);
    let nl;
    while ((nl = lineBuf.indexOf("\n")) >= 0) {
      const line = lineBuf.slice(0, nl).trim(); lineBuf = lineBuf.slice(nl + 1);
      if (!line) continue;
      try { broadcast(server, JSON.parse(line)); }
      catch { /* abaikan baris non-JSON / arahkan ke log */ }
    }
  }
}

function bootSim(server: Bun.Server) {
  if (proc) return;
  proc = Bun.spawn(["firmware/build/targets/simulator/kairo-sim"], {
    env: { ...process.env, KAIRO_SIM_JSON: "1" },
    stdin: "pipe", stdout: "pipe", stderr: "inherit",
    onExit(_p, code) { broadcast(server, { type: "sim_exit", code }); proc = null;
      if (code === 75) bootSim(server); /* restart contract */ }
  });
  pumpStdout(server);
}

const server = Bun.serve({
  routes: { "/": index },
  fetch(req, server) {
    if (new URL(req.url).pathname === "/ws" && server.upgrade(req)) return;
    return new Response("Kairo Simulator");
  },
  websocket: {
    open(ws) { ws.subscribe("sim"); ws.send(JSON.stringify({ type: "hello" })); },
    message(ws, raw) {
      const msg = JSON.parse(String(raw));
      if (msg.cmd === "boot")     return bootSim(ws.data?.server ?? server);
      if (msg.cmd === "shutdown") return void proc?.stdin?.write('{"cmd":"shutdown"}\n');
      if (msg.cmd === "restart")  return void proc?.stdin?.write('{"cmd":"restart"}\n');
      // command lain (inject_event/wifi_*) diteruskan apa adanya ke binary
      proc?.stdin?.write(JSON.stringify(msg) + "\n");
    },
  },
});
console.log(`Kairo Simulator → http://localhost:${server.port}`);
```

> Detail API (`server.publish`, akses `server` di handler, tipe `Subprocess`) sesuaikan dengan versi Bun aktual — lihat `node_modules/bun-types/docs`. Pola di atas adalah cetak biru.

### React UI

```text
packages/simulator/
├─ index.ts          # server + relay (di atas)
├─ index.html        # <script src="./frontend.tsx">
├─ frontend.tsx      # WS client + layout + store
├─ components/
│  ├─ LogsPanel.tsx
│  ├─ EventsPanel.tsx
│  ├─ ServicesPanel.tsx
│  └─ ControlsPanel.tsx
└─ lib/useSimSocket.ts   # hook: connect WS, dispatch by msg.type
```

- `useSimSocket`: buka `ws://localhost:<port>/ws`, simpan arrays `logs[]`, `events[]`, map `services{name→state}`, `system`, dan status koneksi. Kirim command via `ws.send`.
- **LogsPanel**: tabel/stream log; filter checkbox per level (TRACE..FATAL); auto-scroll; tampilkan `component`, `message`, fields.
- **EventsPanel**: stream event (`name`, payload, ts).
- **ServicesPanel**: daftar service + state berwarna (Running=hijau, Failed=merah, Stopped=abu). Update dari pesan `service`.
- **ControlsPanel**: tombol **Boot / Shutdown / Restart**, dan **Inject Event** (form: name + key/value) → kirim `{"cmd":"inject_event",...}`. Bonus (opsional): toggle WiFi (`wifi_connect`/`wifi_disconnect`).
- Header: indikator koneksi WS + status sim (running/stopped, dari `ready`/`sim_exit`).

### package.json (packages/simulator)

```jsonc
{
  "scripts": {
    "dev":   "bun --hot ./index.ts",
    "start": "bun ./index.ts"
  },
  "dependencies": { "react": "^19", "react-dom": "^19" }
}
```

---

## Tasks

- [ ] Server Bun: `Bun.serve` + WebSocket + serve HTML.
- [ ] Spawn manager: boot/shutdown/restart + parse stdout JSON-lines + relay command ke stdin + handle exit/restart code.
- [ ] Penanganan "binary belum di-build" (pesan error rapi, server tetap hidup).
- [ ] React shell + `useSimSocket` (konsumsi semua `type`).
- [ ] LogsPanel (+ filter level), EventsPanel, ServicesPanel, ControlsPanel (Boot/Shutdown/Restart/Inject Event).
- [ ] Indikator status koneksi & sim.
- [ ] `package.json` scripts + deps React; `bun install`.

## Acceptance criteria

- `bun run dev` di `packages/simulator` membuka server; browser ke URL menampilkan UI.
- Klik **Boot** → `kairo-sim` ter-spawn; Logs mulai mengalir, Services menampilkan driver+ClockService jadi **Running**, Events menampilkan `SystemReady`+`ClockTick`.
- Filter level di Logs bekerja.
- **Inject Event** mengirim event yang muncul di EventsPanel.
- **Shutdown** → services jadi Stopped lalu proses keluar; **Restart** → proses re-spawn & stream lanjut.
- Membuka UI tanpa binary ter-build → pesan jelas, server tidak crash.

## How to verify

```bash
bun run build:firmware                 # pastikan binary ada
cd packages/simulator && bun run dev   # buka URL yang tercetak, klik Boot
```

## Risks / notes

- Path binary di-hardcode relatif root; jika Bun dijalankan dari dir lain, resolusi path bisa meleset → pakai path absolut via `import.meta.dir`/`process.cwd()` dengan hati-hati. Dokumentasikan asumsi "jalankan dari root atau dari packages/simulator".
- Pastikan kontrak exit code `restart` sinkron dengan stage 09.
- Hindari memicu dialog/alert di browser (lihat panduan harness). Cukup render state.
