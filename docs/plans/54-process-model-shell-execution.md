# 54 — Process Model & Shell App Execution (Linux-style)

> App jalan sebagai **proses** di atas shell (Plan 44–46): argv, stdin, stdout,
> stderr, exit code, cwd/env. Mendukung pipe. Parsing arg ala commander = userspace.

- Status: ✅ Implemented — ProcessContext/IInputStream/IOutputStream/Pipe/ProcessHost/ProcessManager all implemented; shell `run <app> [args…]` command done with single-process and `A | B` pipe chaining; exit code propagated to `session.lastExit`
- Depends on: 44 (CLI shell), 45 (multi-session), 46 (process monitor)
- Blocks: 55, 56/57/58

---

## Goals

- Mendefinisikan **ProcessContext** (argv[], stdin stream, stdout/stderr sink,
  cwd/env, exit(code)) yang runtime-agnostic.
- Integrasi launch-dari-shell (`run <app> args…`), exit code, history.
- Dukungan **pipe** (`appA | appB`) lewat stdio stream.
- Memetakan stdio ke tiap runtime (C native, WASM via WASI, JS via `process`).

## Keputusan

- Kernel hanya menyediakan argv + stdio + exit (~5 syscall); **parser arg
  (commander/clap/getopt) hidup di userspace/SDK**, bukan kernel.
- WASM dapat stdio POSIX **gratis via WASI** (`fd_read/fd_write/args_get/proc_exit`).
- stdin = stream (untuk pipe + CLI remote); app on-device boleh abaikan.

---

## Latar belakang

Shell device sudah matang lewat Plan 44–46, tapi **belum ada konsep "proses"** —
shell hanya bisa men-dispatch command built-in, belum bisa me-launch *app* sebagai
proses dengan argv/stdio/exit.

**Yang sudah ada (Plan 44–46):**

- `CliService` = registry command stateless-terhadap-kernel: `execute(line,
  session)` parse → push history → dispatch ke `Handler(CliContext&)`
  (`core/include/nema/services/cli_service.h:37`, `:62`). Tiap handler menerima
  `CliContext{ args, out, session }` (`cli_service.h:62-66`).
- `CliSession` memegang state per-koneksi: `id`, `cwd` (default `/`, di atas VFS),
  `history` (ring ~32), `out` sink (`cli_service.h:50-59`). **`cwd` dan output sink
  inilah yang akan diwariskan ke proses anak.**
- `CliSessionManager` (Plan 45) memiliki banyak sesi independen, dikunci `sid`
  1-byte (`cli_service.h:73-82`), dimiliki Runtime via `rt.cliSessions()`.
- `RemoteService` merutekan channel CLI per-`sid`: frame masuk `[sid][line]`,
  keluar `[sid][text]` (output), `[sid][0x04]` (EOT), `[sid][0x01][cwd]` (prompt)
  (`core/include/nema/services/remote_service.h:51-52`, `:72`). **Pola framing ini
  yang nanti membawa stdout/stderr proses balik ke terminal yang benar.**
- `ps` (Plan 46) sudah mengenumerasi services + apps (`AppHostManager::
  foregroundName()`/`pausedName()`) + sesi CLI — fondasi process-monitor.

**Yang sudah ada (eksekusi app, Plan 22):**

- `IApp` = `id()`, `name()`, `run(AppContext&)`, `fullscreen()`, `stackBytes()`
  (`core/include/nema/app/app.h:16-30`). App jalan di **thread sendiri**.
- `AppContext` adalah satu-satunya permukaan yang disentuh app, **tapi murni
  UI-sentris**: `canvas()`, `present()`, `nextInput()/waitInput()`,
  `requestExit()/shouldExit()`, `runtime()` (`core/include/nema/app/
  app_context.h:14-38`). **Tidak ada argv, tidak ada stdin/stdout/stderr, tidak ada
  cwd/env, tidak ada exit code** — semua yang dibutuhkan model proses Unix absen.
- `AppHost` (`core/src/app/app_host.cpp`) menjahit IApp ke ViewDispatcher: ia
  sekaligus `IScreen` (GUI thread) **dan** `AppContext` (app thread). Thread
  di-spawn di `enter()` (`app_host.cpp:79`), `run()` dipanggil di `threadEntry`
  (`app_host.cpp:159-162`), dan saat thread app selesai `tick()` mem-`pop()`
  view-nya (`app_host.cpp:142-148`). **Tidak ada exit code yang ditangkap** — app
  cuma "selesai".
- `AppHostManager` me-launch app dan menegakkan kebijakan **single-slot +
  pause/resume** Plan 22 (`core/include/nema/app/app_host_manager.h:18-44`).
- `AppRegistry::launch(id)` → `AppHostManager::launch()` (`app_registry.h:62`).
  Launch hari ini **selalu dari launcher UI**, tak pernah dari shell, dan tak
  membawa argumen.

**Celah yang ditutup Plan 54:** memisahkan *primitif proses* (argv/stdio/cwd/env/
exit) dari *permukaan UI* (canvas/present — pindah ke Plan 55), lalu menyambungkan
launch-proses ke shell sehingga `run <app> args…` jadi nyata, dengan exit code dan
pipe. `AppContext` yang gemuk sekarang dipecah: **`ProcessContext` (Plan 54)** +
**surface UI yang diambil terpisah (Plan 55)**.

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Argumen | `loader_start(name, args)` — **satu string mentah**, app parse sendiri (`applications/services/loader/loader.c:187`) | App metadata + install, tak ada argv per-launch | **`argv[]` ter-tokenisasi** oleh shell; parser (commander/clap) di userspace/SDK |
| stdio | Tak ada pipe; CLI command nulis ke `Cli*` (satu arah) | Tak ada stdio proses | **stdin/stdout/stderr = stream**, mendukung pipe `A \| B` |
| Thread proses | `FuriThread` per-app, satu app foreground | State machine `APP_STATE_NEW→RUNNING→STOPPED/ERROR/FAILED` (`app_manager.h:72`) | Thread per-proses (sudah ada di `AppHost`), + exit code ditangkap |
| Exit code | Callback app `void` — tak ada kode keluar | `APP_STATE_*` enum, bukan integer exit | **`int exit(code)`**, shell simpan `$?` |
| Launch dari shell | Loader CLI `loader_cli.c` (`loader open <name>`) | `app start <name>` shell-first | **`run <app> args…`** (atau nama telanjang ala PATH), warisi cwd sesi |
| Kernel surface | Monolit furi | Driver registry + cap_mask | **Kernel ~5 syscall** (argv+stdio+exit); sisanya userspace |

Flipper membuktikan model "loader + thread + args string" cukup untuk MCU; AkiraOS
membuktikan state-machine lifecycle proses berguna untuk introspeksi. Palanu
mengambil keduanya **plus** stdio-stream sungguhan (pipe) dan exit code integer —
sesuatu yang **tak satu pun** dari dua referensi punya, dengan harga rendah karena
thread-per-app + per-sid output routing sudah ada.

---

## Desain teknis

### 1. Stream primitif (`core/include/nema/proc/stream.h`)

Pipe dan stdio butuh abstraksi byte-stream tunggal yang runtime-agnostic. Sengaja
seminimal mungkin (byte in/out + EOF), bukan iostream.

```cpp
namespace nema {

// Sumber byte (stdin / read-end pipe). read() non-blocking-friendly:
//   > 0  jumlah byte terbaca
//   = 0  EOF (writer sudah tutup)
//   < 0  belum ada data sekarang (would-block); coba lagi nanti
struct IInputStream {
    virtual ~IInputStream() = default;
    virtual int  read(uint8_t* buf, size_t n) = 0;
    virtual bool eof() const = 0;
    // Helper userspace-able: baca satu baris (sampai '\n' / EOF). Boolean false = EOF.
    bool readLine(std::string& out);
};

// Sink byte (stdout / stderr / write-end pipe).
struct IOutputStream {
    virtual ~IOutputStream() = default;
    virtual void write(const uint8_t* buf, size_t n) = 0;
    void writeStr(const std::string& s) { write((const uint8_t*)s.data(), s.size()); }
    virtual void flush() {}
    virtual void close() {}     // tutup write-end → reader lihat EOF
};

} // namespace nema
```

**Adapter ke shell:** sink stdout/stderr default membungkus `CliSession::out`
(`cli_service.h:55`) — jadi `println` proses keluar lewat jalur per-`sid` yang sama
seperti output command biasa (`RemoteService::sendCli`, `remote_service.h:72`).
stderr boleh ditandai agar Forge bisa mewarnai berbeda (mis. prefix opsional pada
frame), tapi defaultnya menyatu dengan stdout (ala terminal).

### 2. `ProcessContext` (`core/include/nema/proc/process_context.h`)

Permukaan kernel yang dilihat proses. **Runtime-agnostic**: native C++, WASM, dan
JS semua memetakan ke struktur yang sama. Menggantikan bagian non-UI `AppContext`.

```cpp
namespace nema {

class Runtime;

class ProcessContext {
public:
    virtual ~ProcessContext() = default;

    // — argv —  argv[0] = nama yang dipakai memanggil (ala Unix). Parsing flag
    // (commander/clap/getopt) hidup di USERSPACE/SDK, bukan di sini.
    virtual const std::vector<std::string>& args() const = 0;

    // — environment & cwd —  diwarisi dari CliSession peluncur (cwd: cli_service.h:52).
    virtual const std::string& cwd() const = 0;
    virtual const char*        env(const char* key) const = 0;   // nullptr jika tak ada

    // — stdio (fd 0/1/2) —  selalu ada; on-device app boleh abaikan stdin.
    virtual IInputStream&  in()  = 0;     // stdin  (pipe source / kosong)
    virtual IOutputStream& out() = 0;     // stdout (→ CliSession.out / pipe)
    virtual IOutputStream& err() = 0;     // stderr

    // — exit —  satu-satunya "syscall" terminasi. run() harus kembali segera
    // setelah shouldExit() true (pola sama seperti AppContext sekarang).
    virtual void requestExit(int code) = 0;
    virtual bool shouldExit() const = 0;
    virtual int  exitCode()   const = 0;

    virtual Runtime& runtime() = 0;       // TaskRunner, clock, log, events
};

} // namespace nema
```

**Tanda tangan app berubah:** `IApp::run(AppContext&)` → `IApp::run(ProcessContext&)`
(`app.h:20`). App headless cukup ini. App UI memperoleh surface **terpisah** lewat
API Plan 55 (`aether::createSurface(ctx, …)`), bukan dari `ProcessContext` —
itulah inti pemisahan "proses default headless, angkat window opsional".

> **Migrasi `AppContext`:** `canvas()/present()/nextInput()/waitInput()` pindah ke
> `ISurface` (Plan 55). `requestExit()/shouldExit()/runtime()` naik ke
> `ProcessContext` (di sini, + exit code). `AppContext` lama dihapus setelah
> `ComponentApp`/`AppHost` dimigrasi.

### 3. `ProcessHost` — thread + konteks (`core/src/proc/process_host.cpp`)

`AppHost` hari ini menyatukan **tiga** peran: thread proses, `AppContext`, dan
`IScreen` (jembatan compositor). Plan 54 mengekstrak peran proses jadi
`ProcessHost`; jembatan UI (`IScreen` + buffer + present) menjadi `Surface`
Plan 55 yang **hanya dibuat saat app mengangkat window**.

```cpp
class ProcessHost : public ProcessContext {
public:
    ProcessHost(Runtime& rt, IApp& app, ProcessSpec spec);
    void start();                  // spawn thread → app.run(*this)  (app_host.cpp:79 analog)
    bool finished() const;         // thread keluar  (app_host.cpp:143 analog)
    int  join();                   // tunggu + kembalikan exit code
    // ProcessContext impl … (args/cwd/env/stdio/exit/runtime)
private:
    Thread thread_;                // per-proses, stack dari app.stackBytes() (app.h:29)
    std::atomic<int>  exitCode_{0};
    std::atomic<bool> exitReq_{false};
    /* argv, env, cwd, stdin/out/err streams */
};

struct ProcessSpec {              // dirakit oleh shell saat launch
    std::vector<std::string> argv;
    std::string              cwd;     // = CliSession.cwd
    IInputStream*            stdin_;  // pipe read-end / kosong
    IOutputStream*           stdout_; // CliSession.out / pipe write-end
    IOutputStream*           stderr_; // CliSession.out
};
```

`tick()`-detect-exit + `pop()` yang sekarang di `AppHost::tick`
(`app_host.cpp:142-148`) hanya relevan untuk proses **ber-surface**; untuk proses
headless, `ProcessManager` cukup men-`join()` lalu mengambil exit code.

### 4. Pipe (`core/src/proc/pipe.cpp`)

`appA | appB`: satu `Pipe` = ring byte SPSC dengan sinyal EOF. stdout A → write-end,
stdin B → read-end. Kedua proses jalan di thread masing-masing (sudah gratis).

```cpp
class Pipe {
public:
    IOutputStream& writer();      // diberikan ke stdout proses kiri
    IInputStream&  reader();      // diberikan ke stdin proses kanan
    // writer().close() → reader().eof() == true (proses kanan baca habis lalu keluar)
};
```

Aliran: shell parse `A | B` → buat `Pipe p` → `A.stdout=p.writer()`,
`B.stdin=p.reader()`, `B.stdout=session.out` → launch A & B → saat A `exit`, host A
`p.writer().close()` → B lihat EOF → B selesai. **Exit code pipeline = exit code
stage terakhir** (semantik bash). Rantai >2 = N-1 pipe.

### 5. Integrasi shell (`run` + exit code)

Command baru di `registerCoreCliCommands` (`cli_service.h:87`):

```
run <app> [args…]          → launch app sebagai proses foreground, tunggu exit
<app> [args…]              → bare-name lookup ala PATH (registry) = gula untuk `run`
<A args> | <B args>        → pipeline
echo $?                    → exit code proses terakhir
```

- Shell men-tokenisasi baris → `argv[]`, resolve app lewat `AppRegistry`
  (`app_registry.h:62`), rakit `ProcessSpec` (cwd = `ctx.session.cwd`, stdout =
  `ctx.out`), launch via `ProcessManager`.
- **Exit code** disimpan di `CliSession` (field baru `lastExit`), diekspos sebagai
  `$?`. `history` (sudah ada, `cli_service.h:54`) tak berubah.
- **Headless vs UI:** proses yang **tak** mengangkat surface streaming stdout ke
  sesi dan shell menunggu exit (sinkron, ala command biasa). Proses yang
  **mengangkat surface** (Plan 55) menjadi app foreground di display lewat
  `AppHostManager` single-slot Plan 22 — shell kembali ke prompt segera (app hidup
  di layar). Penentu: app memanggil `createSurface()` atau tidak.
- `&` (background job) + job control = **non-goal** sekarang (dicatat).

`ProcessManager` (tipis, dimiliki Runtime — `rt.processes()`) memegang tabel proses
hidup → langsung memberi `ps` (Plan 46) baris PROCESSES dengan PID + exit-state,
melengkapi baris APPS/SESSIONS yang sudah ada.

### 6. Pemetaan stdio per-runtime

| Runtime | argv | stdin | stdout/stderr | exit |
|---|---|---|---|---|
| **C native** (built-in `IApp`) | `ctx.args()` langsung | `ctx.in()` | `ctx.out()/err()` | `ctx.requestExit(code)` |
| **WASM** (Plan 56/57) | WASI `args_get` → `ctx.args()` | WASI `fd_read(0)` → `ctx.in()` | WASI `fd_write(1/2)` → `ctx.out()/err()` | WASI `proc_exit(code)` → `requestExit` |
| **JS** (Plan 37 host, `.kapp`) | `process.argv` | `process.stdin.on('data')` | `process.stdout.write` | `process.exit(code)` |

- **WASM gratis via WASI:** host mengimplementasikan 5 import WASI
  (`args_get/args_sizes_get`, `fd_read`, `fd_write`, `proc_exit`) yang diteruskan
  ke `ProcessContext`. Toolchain Rust/C standar "merasa" ini POSIX biasa — itulah
  alasan kernel cukup ~5 syscall.
- **JS `process`:** host JS (Plan 37) menyuntik objek ambient `process` di samping
  `nema` (`packages/nema-app-sdk/src/system.ts:50`): `process.argv/env/cwd()`,
  `process.stdin` (EventEmitter byte), `process.stdout/stderr.write`,
  `process.exit(code)`. Semua delegasi ke `ProcessContext` underlying. **Parser
  arg (commander/yargs) = paket npm biasa di dalam `.kapp`,** bukan device.
- **C native:** tak butuh shim — `ProcessContext` adalah API-nya.

---

## Fase

- [ ] **Fase 1 — Stream + ProcessContext (core, headless).** `IInputStream`/
      `IOutputStream` + adapter `CliSession.out`. `ProcessContext` + `ProcessHost`
      (ekstrak peran thread/exit dari `AppHost`). `IApp::run(ProcessContext&)`.
      Migrasi `ComponentApp` (UI-nya sementara pakai shim surface in-place). Host
      unit test: argv tokenisasi, stdout→sink, exit code.
- [ ] **Fase 2 — `run` + exit code + `ProcessManager`.** Command `run`/bare-name di
      `registerCoreCliCommands`; resolve via `AppRegistry`; cwd diwarisi dari sesi;
      `$?`. `ProcessManager` + baris PROCESSES di `ps` (Plan 46). Test: launch
      headless app dari sesi, exit code benar, isolasi antar-sesi (Plan 45).
- [ ] **Fase 3 — Pipe.** `Pipe` SPSC + EOF; parser `A | B` di shell; exit =
      stage terakhir. Test host: `gen | filter` dua app native, EOF propagasi,
      rantai 3-stage.
- [ ] **Fase 4 — WASI stdio bridge (sinkron dgn Plan 56/57).** Wire 5 import WASI ke
      `ProcessContext`. Smoke: app Rust/C `cat`-style baca stdin→stdout.
- [ ] **Fase 5 — JS `process` (sinkron dgn Plan 37/58).** Inject objek `process`;
      `argv/stdin/stdout/exit`. Smoke `.kapp` headless yang echo argv + baca stdin.

**Build/uji:** host + WASM tiap fase; ESP32 build-only Fase 2 & 4.

---

## File yang disentuh

- **Baru:** `core/include/nema/proc/stream.h`, `core/include/nema/proc/
  process_context.h`, `core/include/nema/proc/process_host.h` +
  `core/src/proc/process_host.cpp`, `core/src/proc/pipe.cpp`,
  `core/include/nema/proc/process_manager.h` + `.cpp`.
- **Diubah:** `core/include/nema/app/app.h` (`run(ProcessContext&)`),
  `core/src/app/app_host.cpp` (ekstrak peran proses → `ProcessHost`; sisakan
  jembatan surface untuk Plan 55), `core/include/nema/app/component_app.h` +
  `.cpp` (migrasi loop), `cli_service.{h,cpp}` (`CliSession.lastExit`, command
  `run`/`$?`/pipe di `registerCoreCliCommands`), `runtime.{h,cpp}`
  (`processes()`), `46-process-monitor` `ps` (baris PROCESSES).
- **Runtime UI:** host JS (Plan 37) inject `process`; WASI host (Plan 56/57) wire fd.
- **SDK:** `packages/nema-app-sdk/src/system.ts` (deklarasi tipe `process`).

---

## Test

- **Unit (host):** tokenisasi argv (quoting), stdout/stderr→sink, exit code
  round-trip, `cwd` diwarisi dari `CliSession`, `$?` benar, isolasi antar-`sid`
  (sesi A `run` tak bocor ke B), pipe EOF + exit-code stage terakhir, rantai 3.
- **WASM:** `run` headless app dari Forge CLITerminal; stdout muncul per-`sid`;
  exit code; pipe dua app.
- **ESP32:** build-only (Fase 2 & 4) — pastikan thread/stack `ProcessHost` waras.

---

## Risiko & mitigasi

- **Refactor `AppHost` berisiko regresi UI.** → Fase 1 hanya *mengekstrak* peran
  proses; jembatan UI tetap di tempat lewat shim sampai Plan 55. Test parity
  ComponentApp host+WASM tiap langkah.
- **Pipe = deadlock/backpressure.** Ring SPSC kapasitas tetap; writer full →
  blok/would-block, reader EOF saat writer close. Mitigasi: buffer cukup besar +
  semantik would-block jelas; uji rantai panjang.
- **stdin pada device headless tak punya sumber.** → stdin default = stream kosong
  ber-EOF; app harus toleran (Keputusan: "app on-device boleh abaikan stdin").
- **Thread-per-proses mahal di MCU.** → tetap single-slot foreground (Plan 22);
  proses headless berumur pendek (run-to-exit). Job control/background = non-goal.
- **WASI/JS exit semantics beda.** → Satu titik konvergensi `ProcessContext::
  requestExit(code)`; shim runtime hanya menerjemahkan ke sana.

---

## Yang sengaja TIDAK dikerjakan (sekarang)

- **Background jobs (`&`) + job control** (fg/bg/jobs) — shell sinkron dulu.
- **Sinyal POSIX** (SIGINT/SIGTERM) di luar `requestExit` — `Ctrl+C` abort
  menyusul saat ada command long-running (lihat Plan 44 Fase 3 deferred).
- **fork/exec sejati** — proses dibuat oleh shell dari registry, bukan oleh proses
  lain. Tak ada hierarki parent/child selain pipeline.
- **Quota/sandbox per-proses** (Akira `cap_mask`) — plan App Permissions terpisah.
