# 0010 — Internal SRAM is the scarce resource: WiFi buffer diet over PSRAM offload

- Status: Accepted
- Date: 2026-06-23
- Plan: [88](../plans/88-remote-protocol-v2.md)
- Board: skyrizz-e32 (ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB Octal PSRAM, ~230 KB internal SRAM)
- References studied: `refs/Flipper-Zero-ESP32-Port` (same SoC, ships this exact profile), `refs/AkiraOS`.

## Context

While stabilising the remote file channel (`cp` to `/sd`), a chain of "fixes" kept
shifting the failure instead of removing it: SD write `not enough mem`, then `f_open`
failures, then — once enlarged stacks/reserves were applied — WiFi spamming
`wifi:m f null`, mDNS `Cannot allocate memory`, and the WebSocket server's
`httpd_start failed` (which silently broke network `cp`/connect). A boot-log RAM
audit revealed the real picture, and the user's hypothesis ("maybe it's not just the
SD — something else eats the RAM") was correct.

The ESP32-S3 has two heaps with very different properties:
- **PSRAM (8 MB)** — plentiful, but **not DMA-capable** and slower.
- **Internal SRAM (~230 KB)** — the **only DMA-capable** RAM, and **fixed in silicon**
  (cannot be added).

Every DMA-driven subsystem competes for that one scarce pool: WiFi radio buffers,
the SD (SDSPI) DMA bounce buffer, I2S audio, the LCD, plus all FreeRTOS task stacks,
the mDNS task, and the httpd/WS server. Measured steady state with WiFi connected:
**~9 KB internal free.** At that margin, *any* ~4 KB internal/DMA allocation fails
intermittently — which is exactly what SD, mDNS, and httpd all hit.

The earlier attempts made it worse: a 32 KB `cdc_rx` stack and a 64 KB DMA reserve
added ~56 KB of internal pressure and tipped WiFi itself into allocation failure.

## Decision

**Internal SRAM is the budget to optimise. Relocate what doesn't need DMA, and keep
DMA users frugal — do not try to "buy headroom" by reserving more internal RAM.**

1. **WiFi buffer diet (the big lever), mirroring Flipper-ESP32's shipping profile:**
   - `ESP_WIFI_DYNAMIC_TX_BUFFER` instead of 16 permanent static TX buffers (~26 KB
     internal reclaimed; TX buffers are now allocated on demand).
   - `ESP_WIFI_STATIC_RX_BUFFER_NUM` 16 → 10; `ESP_WIFI_RX_BA_WIN` 16 → 6.
   - **Do NOT set `SPIRAM_TRY_ALLOCATE_WIFI_LWIP`.** IDF Kconfig makes it mutually
     exclusive with dynamic TX (a dynamic TX buffer could land in non-DMA PSRAM), so
     enabling it *forces* the 26 KB of static TX buffers back into internal RAM. The
     dynamic-TX profile frees far more than try-allocate ever saved.
2. **Keep the DMA reserve at the 32 KB default** (raising it starves WiFi/mDNS/httpd).
3. **Keep task stacks frugal:** `cdc_rx` at 12 KB, not 32 KB. (Very large single SD
   writes that need a deeper FATFS-flush stack are an accepted edge case; app-sized
   files and LittleFS are comfortable.)
4. **Keep FATFS work buffers in PSRAM** (decision 0009 context) — moving them internal
   exhausted SRAM and broke `f_open`.

## Consequences

- Internal free SRAM went from **~9 KB → ~130 KB** steady-state with WiFi connected.
- **SD `/sd` read + write is now reliable** (3/3 round-trips byte-identical, vs
  intermittent before) — the DMA bounce always allocates.
- **Network works:** the WS server's `httpd_start` succeeds and **mDNS resolves
  `skyrizz-e32.local`**, so Forge-CLI/Web can remote over WiFi again.
- WiFi throughput drops slightly (dynamic TX, 6-deep block-ack window) — irrelevant
  for remote-control + file transfer, and the trade Flipper already chose on this SoC.
- General principle for this board going forward: **adding a feature that needs DMA or
  a task stack spends from a ~230 KB silicon-fixed pool.** Audit internal-SRAM free
  (`free_heap − free_psram`) when integrating new peripherals; prefer PSRAM for any
  buffer that isn't DMA'd; never solve an internal-RAM shortage by reserving more of it.
