# 45 — Multi-Session CLI (independent shells)

> Banyak sesi CLI hidup bersamaan, **saling terisolasi** — sesi remote A, remote
> B, dan (nanti) TTY lokal di device masing-masing punya `cwd`/`history` sendiri;
> satu sesi tak mengganggu yang lain. Mirror model Flipper (`CliRegistry` bersama +
> `CliShell` per koneksi), tapi **lewat satu link** pakai **session-id** di channel
> CLI — jadi bahkan satu kabel bisa membawa >1 sesi (Flipper strict per-pipe).

- Status: 🟢 Fase 1 + 2 done & build-verified (host 10/10 + ESP32 dev-board + WASM + Forge type-check clean). Fase 3 (Forge "new session" button + local TTY) optional, deferred.
- Milestone: M12 (Runtime Foundation)
- Depends on: Plan 35 (PLP), Plan 44 (CliSession + CliService registry)
- Catatan: keputusan **(A) session-id** (bukan per-transport) dipilih agar mux PLP
  tetap utuh dan hasilnya ≥ Flipper.

---

## Latar belakang

Plan 44 memberi `CliSession` (cwd + history), tapi RemoteService memegang **satu**
sesi karena mux menggabungkan semua transport jadi satu link. Flipper
(`cli_vcp.c`) spawn **`CliShell` per koneksi** di atas registry bersama
(`CliRegistry`). Pemetaan ke Kairo: `CliService` = registry ✅, `CliSession` =
shell ✅ — yang kurang: **pengelola banyak sesi + routing per-sesi**.

Karena PLP di-mux, "koneksi" tak punya identitas di sisi gabungan. Solusi: bawa
**session-id 1-byte** di tiap frame channel CLI. Device punya `CliSessionManager`
yang get-or-create sesi per-id dan merutekan output kembali dengan id yang sama.

## Protokol channel CLI (revisi, both directions)

Tiap frame CLI sekarang berformat `[sid:1][rest...]`:
- host→device: `[sid][command line]`
- device→host: `[sid][text]` (output), `[sid][0x04]` (EOT), `[sid][0x01][cwd]` (prompt)

`sid` dipilih klien (Forge: per-terminal). Device get-or-create sesi untuk `sid`.

---

## 1. Goal

1. **`CliSessionManager`** (core): `CliSession& get(uint8_t sid, makeOut)`, `list()`,
   `remove(sid)`, `clear()`. Dimiliki Runtime (`rt.cliSessions()`), supaya sumber
   sesi apa pun (PLP remote, TTY lokal nanti) memakai pengelola yang sama.
2. **RemoteService** merutekan channel CLI per-`sid`: get-or-create sesi (out sink
   yang mem-prefix `sid`), eksekusi di sesi itu, EOT+prompt juga ber-`sid`. Saat
   link putus → `clear()`.
3. **Isolasi**: tiap sesi punya `cwd`/`history` sendiri — `cd` di sesi A tak
   mengubah sesi B.
4. **`sessions`** command: daftar sesi aktif (sid, cwd, jumlah history).
5. **Forge**: tiap `CliTerminal` punya `sid`; kirim dengan prefix, filter masuk
   per-`sid`. (Satu terminal sekarang; tombol "sesi baru" = polish nanti.)
6. Teruji host + WASM; build ESP32 OK.

**Non-goal:** TTY lokal interaktif di device (butuh input keyboard lokal — belum
ada), UI multi-terminal di Forge (cukup satu untuk sekarang), `ps`/process
monitor (concern terpisah).

---

## 2. Fase pengerjaan

- [x] **Fase 1 — Device: session manager + per-sid routing.** `CliSessionManager`,
      `rt.cliSessions()`, RemoteService sid-framing, `sessions` command. Host test
      isolasi (cwd sesi A ≠ B).
- [x] **Fase 2 — Forge: per-terminal sid.** RemoteSession/simStore `sendCli(sid,line)`
      + parse sid; CliTerminal pakai sid + filter. WASM rebuild.
- [~] **Fase 3 — Polish (opsional).** Tombol "new session" di Forge; TTY lokal saat
      input keyboard device ada.
