# 44 — CLI Shell & Per-Connection Sessions

> Naikkan CLI dari "command interpreter stateless" jadi **shell sungguhan dengan
> sesi per-koneksi** — setara Flipper, lalu lewati. Tiap koneksi (USB/BLE/WASM
> cable) punya **`CliSession`** sendiri: history device-side, **working directory
> (`cwd`)** di atas VFS, dan output sink-nya. Handler menerima **`CliContext`**
> sehingga command bisa stateful (`cd`/`pwd`, `ls`/`cat` relatif cwd). Sesi
> dibuka saat link `onReady`, ditutup saat `onDisconnect` (Plan 42 F4).
>
> Benchmark: Flipper punya sesi shell per-koneksi (thread + ANSI + lifecycle)
> TAPI command-nya **stateless** (tak ada cwd). Palanu menambah **konteks sesi
> stateful** + sesi **transport-agnostic lewat mux** (USB+BLE bareng, tiap
> koneksi terisolasi) — itu yang "lebih baik".

- Status: 🟢 Fase 1 done & build-verified (host 10/10 + ESP32 dev-board + WASM green). Fase 2-4 (multi-conn isolation, streaming/Ctrl+C, Forge cwd prompt) deferred.
- Milestone: M12 (Runtime Foundation)
- Depends on: Plan 35 (KLP remote), Plan 38 (VFS/filesystem), Plan 42 Fase 4
  (`LinkService::onDisconnect`)
- Catatan kode: `nema::` (rebrand `palanu` = Plan 41).

---

## Latar belakang

`CliService` sekarang = registry command stateless: `execute(line, out)` →
parse → dispatch. State terminal (history, panah atas/bawah) hidup di **klien**
(Forge `CliTerminal.svelte`), bukan device. Tak ada konteks per-koneksi, tak ada
working directory walau VFS sudah ada. Komentar di `cli_service.h` sendiri sudah
menandai ini sebagai langkah berikutnya.

Flipper (`lib/toolbox/cli/shell/cli_shell.c`) menjalankan **thread shell
per-koneksi** dengan event loop + ANSI parser + lifecycle pipe — tapi command-nya
tetap stateless terhadap filesystem. Itu yang kita setarai lalu lampaui.

---

## 1. Goal

1. **`CliSession`** (`core/include/nema/services/cli_session.h`): state per-koneksi —
   `id`, `history` (ring, device-side), `cwd` (default `/`), `Out` sink, `alive`.
2. **`CliContext`** dioper ke tiap handler: `{ args, out, session, runtime }`.
   Handler lama `(args, out)` bermigrasi ke `(CliContext&)`.
3. **Command stateful** (mengalahkan Flipper): `cd <dir>`, `pwd`, dan `fs ls/cat`
   resolve **relatif `cwd`**; `history` menampilkan riwayat sesi.
4. **Lifecycle**: `RemoteService` membuka `CliSession` saat `link.onReady`, menutup
   saat `onDisconnect`. Tiap frame `Channel.Cli` diumpankan ke sesi koneksi itu.
5. **Multi-koneksi**: mux (USB+BLE) → tiap transport punya sesi sendiri (history &
   cwd terisolasi). Satu device, banyak sesi paralel.
6. Teruji **host + WASM**; build ESP32 OK.

**Non-goal (fase lanjut, dicatat):** ANSI line-editing device-side (Forge sudah
edit di klien; penting hanya untuk serial mentah), pipes/scripting, `Ctrl+C`
abort untuk command streaming panjang, autocomplete RPC.

---

## 2. Arsitektur

```
  Forge / serial / BLE  ──Channel.Cli──▶  RemoteService
                                            │  openSession(onReady) / close(onDisconnect)
                                            ▼
                                        CliSession { id, history, cwd, out }
                                            │  feed(line)
                                            ▼
                                        CliService (registry)  ──dispatch──▶  Handler(CliContext&)
                                                                                  │ ctx.session.cwd, ctx.out, ctx.args
                                                                                  ▼
                                                                              VFS / runtime
```

### 2.1 Types (`core/include/nema/services/cli_session.h`)
```cpp
struct CliSession {
    uint32_t                 id   = 0;
    std::string              cwd  = "/";        // stateful working dir (VFS)
    std::vector<std::string> history;           // device-side, capped
    CliService::Out          out;               // this connection's output sink
    bool                     alive = true;
    void pushHistory(const std::string& line);  // ring, cap ~32
};

struct CliContext {
    const std::vector<std::string>& args;
    const CliService::Out&          out;
    CliSession&                     session;
    Runtime&                        rt;
};
```

### 2.2 CliService (registry) — Handler takes context
```cpp
using Handler = std::function<void(CliContext&)>;            // was (args, out)
void execute(const std::string& line, CliSession& s);        // parse → dispatch w/ ctx
// keep a convenience execute(line, out) that uses a throwaway session.
```

### 2.3 Stateful commands
- `pwd` → `out(session.cwd)`.
- `cd <dir>` → resolve (abs/rel/`..`) against VFS, update `session.cwd` if it exists.
- `fs ls [path]` / `cat <path>` → resolve relative to `session.cwd`.
- `history` → list `session.history`.

### 2.4 Lifecycle in RemoteService
- `onReady` → `shell_.open(id, sink)`; `onDisconnect` → `shell_.close(id)`.
- `Channel.Cli` frame → `cli_->execute(line, session)`; session pushes history.

---

## 3. Fase pengerjaan

- [ ] **Fase 1 — Session core + context refactor.** `CliSession`/`CliContext`;
      migrate `Handler` to `(CliContext&)`; refactor all `registerCoreCliCommands`
      + esp32 `ram` to the context form; `pwd`/`cd`/`history` + cwd-relative
      `fs`/`cat`. Session lifecycle in RemoteService via onReady/onDisconnect.
      Host unit test (history ring, cwd resolve, dispatch).
- [ ] **Fase 2 — Multi-connection isolation.** One CliSession per mux child; verify
      USB + BLE hold independent cwd/history.
- [ ] **Fase 3 — Streaming + abort.** Long commands stream; `Ctrl+C` (a Cli control
      byte) aborts — matches Flipper's interactive feel.
- [ ] **Fase 4 — Forge polish.** Show cwd in the prompt; device-side history opt-in.

---

## 4. Yang sengaja TIDAK dikerjakan (sekarang)

- Device-side ANSI line editing — Forge edits client-side; only matters for a raw
  dumb serial terminal. Defer.
- Pipes / scripting / job control — this is a command shell, not a Unix shell.
- Per-session auth — folded into a future App/Remote Permissions plan.
