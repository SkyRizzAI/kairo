# 09 — Stdio Bridge Protocol (C++ side)

> Jembatan antara binary C++ dan dunia luar **tanpa WebSocket di C++**: telemetry keluar lewat **stdout** sebagai JSON-lines, command masuk lewat **stdin** sebagai JSON-lines. Sisi web (Bun relay) ada di stage 10.

- Status: ☐ Not started
- Milestone: M2/M3
- Depends on: 03, 04, 05, 07 (butuh Logger/EventBus/ServiceManager/boot jalan)
- Blocks: 10

---

## Goal

- Definisikan **protokol JSON-lines** dua arah (kontrak dipakai bersama stage 10).
- Sisi C++:
  - **JsonStdoutSink** (ILogSink) → setiap log jadi 1 baris JSON `{"type":"log",...}`.
  - **Event bridge**: subscribe EventBus `"*"` → emit `{"type":"event",...}`.
  - **Service bridge**: transisi state service → emit `{"type":"service",...}`.
  - **Snapshot** saat boot: `{"type":"system"/"hardware"/"capability"}` + `{"type":"ready"}`.
  - **Command reader**: poll stdin (non-blocking) di `platform.idle()`/`run()` loop → parse command → aksi (`shutdown`, `restart` (via exit code), `inject_event`, `wifi_connect`/`wifi_disconnect`).

## Scope

### In scope

- Spesifikasi protokol (di bawah) — **sumber kebenaran** untuk stage 10.
- Implementasi emit telemetry & baca command di platform/target simulator.
- Non-blocking stdin read di host (POSIX `poll`/`read` pada fd 0) — diletakkan di platform sim (bukan core).
- Mode output: human-readable (ConsoleSink) vs JSON-lines (JsonStdoutSink). Pilih via env/flag (mis. `PALANU_SIM_JSON=1` atau argv `--json`) supaya `run-sim.sh` manual tetap enak dibaca, dan Bun pakai mode JSON.

### Out of scope

- WebSocket apa pun (itu di Bun, stage 10).
- Protokol biner / framing rumit (JSON-lines cukup).

---

## Design

### Transport

- **Core → luar**: tulis ke `stdout`, **satu objek JSON per baris** (`\n`-terminated), di-flush tiap baris.
- **Luar → core**: baca `stdin`, satu JSON per baris.
- Semua field `ts` = epoch ms (dari `IClock::epochMs`).

### Pesan: Core → UI (telemetry)

```jsonc
{"type":"log","ts":1234567890,"level":"INFO","component":"Logger","message":"Initialized","fields":{"k":"v"}}
{"type":"event","ts":...,"name":"SystemReady","payload":{"k":"v"}}
{"type":"service","ts":...,"name":"ClockService","state":"Running"}
{"type":"system","ts":...,"info":{"platform":"simulator","board":"simulator","buildVersion":"0.1.0-mvp", ...}}
{"type":"hardware","ts":...,"items":[{"id":"wifi","kind":"Wifi","detail":"virtual"}]}
{"type":"capability","ts":...,"items":["battery","wifi","networking"]}
{"type":"ready","ts":...}
```

### Pesan: UI → Core (command)

```jsonc
{"cmd":"shutdown"}
{"cmd":"restart"}                          // C++ keluar dgn exit code khusus; Bun yg re-spawn (stage 10)
{"cmd":"inject_event","name":"BatteryChanged","payload":{"level":"42"}}
{"cmd":"wifi_connect","ssid":"Office"}
{"cmd":"wifi_disconnect"}
```

> `boot`/`restart` di level proses ditangani Bun (spawn/kill). Sisi C++ cukup menangani `shutdown` (keluar loop) & `restart` (exit dengan kode yg memberi tahu Bun untuk re-spawn). Command lain → aksi langsung pada driver/eventbus.

### File

```text
firmware/platforms/simulator/
├─ src/json_stdout_sink.cpp      # ILogSink → JSON line
├─ src/telemetry_bridge.cpp      # subscribe event/service → JSON line
├─ src/command_reader.cpp        # non-blocking stdin → parse → dispatch
└─ include/palanu/sim/bridge.h
```

> Semua pakai `nlohmann/json` (vendored) — boleh karena ini **platform**, bukan core. Core tetap bersih.

### Command dispatch (sketsa)

```cpp
void CommandReader::poll(Runtime& rt) {
  // POSIX: if (data available on fd 0) read line(s)
  for (auto& line : readAvailableLines()) {
    auto j = json::parse(line, nullptr, /*throw=*/false);
    if (j.is_discarded()) { rt.log().warn("Bridge","bad command json"); continue; }
    auto cmd = j.value("cmd", "");
    if (cmd == "shutdown") rt.requestShutdown();
    else if (cmd == "restart") { rt.requestShutdown(); g_exitCode = 75; }  // Bun re-spawn
    else if (cmd == "inject_event") rt.events().publish(toEvent(j));
    else if (cmd == "wifi_connect") rt.container().require<IWifiDriver>().connect(j["ssid"].get<std::string>().c_str());
    else if (cmd == "wifi_disconnect") rt.container().require<IWifiDriver>().disconnect();
    else rt.log().warn("Bridge", "unknown cmd", {{"cmd", cmd}});
  }
}
```

- `CommandReader::poll(rt)` dipanggil dari `run()` loop / `platform.idle()`.
- Non-blocking: gunakan `poll(2)` dgn timeout 0 pada fd 0; kalau loop terlalu sibuk, beri sleep kecil (mis. 5–10ms) agar CPU tak 100%.

### Mode output

- `initCore()` pasang sink berdasarkan mode:
  - default / `--human`: `ConsoleSink`.
  - `--json` atau `PALANU_SIM_JSON=1`: `JsonStdoutSink` (+ MemorySink tetap).
- Saat mode JSON, transisi service & event juga di-emit sebagai JSON line (telemetry_bridge aktif), dan snapshot system/hardware/capability + `ready` dikirim setelah `start()`.

---

## Tasks

- [ ] Finalkan & dokumentasikan skema JSON (jadikan acuan stage 10 — boleh duplikasi tabel ini di stage 10).
- [ ] `JsonStdoutSink` (ILogSink → JSON line, flush per baris).
- [ ] `telemetry_bridge`: subscribe `"*"` event + hook transisi service → JSON line. Snapshot system/hardware/capability + `ready`.
- [ ] `command_reader`: non-blocking stdin (POSIX poll/read), parse, dispatch (shutdown/restart/inject_event/wifi_*).
- [ ] Mode switch human vs JSON (flag/env).
- [ ] Panggil `CommandReader::poll` di `run()` loop; hapus batas-waktu shutdown sementara dari stage 07.

## Acceptance criteria

- `PALANU_SIM_JSON=1 ./palanu-sim` mengeluarkan JSON-lines valid (tiap baris parse OK) untuk log, event, service, snapshot, dan `ready`.
- `echo '{"cmd":"shutdown"}' | PALANU_SIM_JSON=1 ./palanu-sim` → proses shutdown bersih (services Stopped) lalu exit 0.
- `inject_event` menghasilkan event yang muncul kembali di stream telemetry.
- `wifi_connect` → emit `event NetworkConnected`.
- Tanpa command, proses tetap hidup (tidak auto-exit) — menunggu `shutdown`.

## How to verify

```bash
BIN=firmware/build/targets/simulator/palanu-sim
# stream telemetry + kirim shutdown setelah 2 detik
( printf '{"cmd":"wifi_connect","ssid":"Office"}\n'; sleep 2; printf '{"cmd":"shutdown"}\n' ) \
  | PALANU_SIM_JSON=1 "$BIN" | head -40
# tiap baris harus JSON valid; ada type=event name=NetworkConnected; diakhiri services Stopped
```

## Risks / notes

- **Interleaving stdout**: pastikan hanya JSON-lines yang ke stdout saat mode JSON (jangan ada `printf` liar). Arahkan pesan diagnostik internal ke stderr bila perlu — Bun hanya parse stdout.
- Non-blocking stdin di macOS: `poll`/`select` pada fd 0 oke saat di-pipe oleh Bun. Tangani EOF (stdin closed) → treat as shutdown.
- `restart` exit code (mis. 75) adalah kontrak dengan Bun (stage 10) — dokumentasikan agar dua sisi sepakat.
