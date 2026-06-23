# 0009 — PLP FILE v2: chunked write, inline file ops, opt-in screen mirror

- Status: Accepted
- Date: 2026-06-23
- Plan: [88](../plans/88-remote-protocol-v2.md)
- Supersedes the relevant parts of the "D5 async file task" decision made during Plan 87 work.

## Context

Pushing a 4.9 KB `.papp.zip` to the device over the USB serial CLI (`palanu cp …
/sd/apps/`) failed with an opaque "status 2", then left the device unresponsive.
Forge **Web** could upload the same file; Forge **CLI** (USB) could not. A full
audit of the remote stack (firmware link/RemoteService, `@palanu/link`, forge-cli,
forge-web, WASM) plus on-hardware tracing found four independent causes — none of
which is the SD card:

1. **Whole-file write in one PLP frame.** `writeFile` sent the entire file as a
   single `[op][len][path][data]` frame. PLP's length field is 16-bit (64 KB cap),
   and more importantly the device USB-Serial/JTAG **HWCDC RX queue defaults to 256
   bytes** — a ~1 KB frame overruns it the moment `cdc_rx` is briefly preempted, so
   bytes are dropped, the frame fails CRC, and it is never processed. OTA already
   chunked (1792 B) for exactly this reason; file write never adopted it.
2. **FIFO-per-opcode reply correlation** (no request id) desynced after any timeout.
3. **An async `file_rx` task** (the earlier "D5" fix, meant to keep `cdc_rx` free
   during slow SD I/O) ran at a low FreeRTOS priority and was starved by the WiFi/GUI
   stack, so it silently failed to drain its queue — file ops timed out. Forge Web
   "worked before" precisely because at that time file ops still ran inline.
4. **The screen mirror streamed unconditionally** once a session was ready, flooding
   the link and starving the inbound file/CLI path — even for a CLI that never wants
   a screen.

## Decision

1. **Chunked, acked, idempotent file write.** New FILE opcodes
   `WriteBegin/WriteData/WriteEnd`. The device dictates the chunk size (1024 B) in
   the WriteBegin ack; each WriteData carries an absolute offset and is acked with
   the device's next expected offset, so a dropped/timed-out chunk is retried
   idempotently. The device buffers chunks in RAM and calls the existing
   `IFileSystem::write` once at WriteEnd (streaming-to-disk per offset is a later
   refinement; the pacing is what fixes the RX overrun).
2. **Request id on every FILE message.** `[op][reqId:2]` on requests, echoed on
   replies. The client correlates by a `Map<reqId>`; an unknown/expired reply is
   ignored rather than mis-matched.
3. **Run file ops INLINE on `cdc_rx`** (the same model the WASM simulator uses).
   The async `file_rx` deferral is removed. With chunked writes each frame is cheap
   (a memcpy; only WriteEnd/List/Read touch storage), so inline handling no longer
   stalls the handshake the way a single whole-file write once did.
4. **Enlarge the device RX buffer**: `HWCDC::setRxBufferSize(4096)` and read in
   1 KB chunks, so a full inbound burst survives brief `cdc_rx` preemption.
5. **The screen mirror is opt-in per session.** Default OFF, reset on each HELLO;
   the host enables it via `SysOp::ScreenStream`. `@palanu/link` auto-subscribes
   only while something listens for `screen` frames — so Forge Web (which renders
   the display) gets the mirror automatically and the CLI never triggers the flood.

## Update (Plan 88 full build) — streaming write, deep-stack crash, heartbeat

Implementing the remaining phases surfaced two more hardware truths on large files
(the happy path was small `.papp.zip`s):

6. **Streaming write to disk (R5).** Buffering the whole file in `xfer_.buf` and
   writing once at `WriteEnd` exhausted/fragmented the heap on big repeated transfers
   and wedged the device. `IFileSystem` gained `writeStreamBegin/Chunk(offset)/End/Abort`;
   SD and LittleFS hold one `FILE*` open across chunks and `fseek` per chunk (so a
   retried chunk overwrites in place). Backends without it (MemFS/WASM) keep the
   buffered fallback. The transfer now carries a device-assigned **xferId** (separate
   from the per-message reqId, which differs every frame) so `WriteData/WriteEnd` bind
   to their transfer and a second client can't append into it.

7. **`cdc_rx` stack must be large because file ops run inline.** The decisive
   large-file bug was a `Guru Meditation (InstrFetchProhibited, PC=0x1)` on the GUI
   core: `cdc_rx` overran its stack in the deep `handleFile → FATFS → SDSPI → DMA`
   path (deeper on big files, where FATFS periodically flushes the FAT), corrupting
   the adjacent heap block holding the GUI `TaskRunner`'s `std::function` queue. Raising
   the `cdc_rx` stack 8K→16K→**32K** fixed it. This is the cost of the inline model
   (decision 3): the RX task carries the full FS call depth, so it needs a deep stack.

8. **Heartbeat timeouts must be generous.** In-protocol liveness (host PING + device
   `livenessTick`) detects dead USB/WASM links, but inline file I/O legitimately stops
   the RX task for seconds. Tight timeouts (6 s) false-positived mid-transfer and
   killed large writes; widened to ~60 s device / 30 s client.

## Consequences

- `cp` over the USB CLI works: 4.9 KB in ~300 ms, repeatable without reset, device
  stays responsive. Files land intact on SD.
- One write path for CLI and Web (shared `@palanu/link`); Web upload also becomes
  chunked and more robust. Web screen mirror unchanged (auto-subscribes).
- Inline file ops mean a genuinely slow `list` of a huge SD directory can again
  delay handshake frames on `cdc_rx` (the problem D5 tried to solve). Acceptable
  now because the common path is fast; if it resurfaces, reintroduce deferral with
  a correctly-prioritised, core-pinned task — not a low-priority one.
- `readFile` is still single-shot (large reads remain 64 KB-capped); chunked read
  is deferred. Auth robustness (token refresh, `waitForAuthorized` fail-safe — the
  flaw that let the CLI proceed unauthorized and silently drop File ops) is tracked
  as Plan 88 Fase 5.
- The opt-in screen model is the first concrete piece of the per-session control
  surface that Plan 88 §7 (session model) builds on.
