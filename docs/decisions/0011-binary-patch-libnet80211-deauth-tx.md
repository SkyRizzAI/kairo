# 11. Weaken libnet80211.a's frame sanity check to allow deauth/disassoc TX

Date: 2026-06-24

## Status

Accepted

## Context

The WiFi Marauder app's **Deauth Flood** consistently failed on hardware with:

```
E (xxxxx) wifi:unsupport frame type: 0c0
```

`0xC0` is the 802.11 deauthentication frame type. We initially assumed the cause
was that `esp_wifi_80211_tx()` needs **promiscuous mode** active to inject
management frames, and added `monitorOpen()` calls before enabling injection
(this *was* needed — it fixed a separate "STA is connecting, cannot set channel"
error and the sniff screens reading 0 frames). But after that fix the log showed
promiscuous mode successfully enabled (`ic_enable_sniffer`) **and the deauth
still failed** with the same `unsupport frame type: 0c0`. So the rejection is
unrelated to promiscuous mode.

Root cause: ESP-IDF's closed-source WiFi blob (`libnet80211.a`) gates
`esp_wifi_80211_tx()` behind an internal `ieee80211_raw_frame_sanity_check()`
that rejects management subtypes `0xC0` (deauth) and `0xA0` (disassoc).

We considered three ways to defeat the check:

1. **Link-time symbol override alone** — define our own
   `ieee80211_raw_frame_sanity_check(){return 0;}`. Fails: the blob's symbol is
   `GLOBAL` + strong, so a second strong definition is a multiple-definition
   error.
2. **Binary-patch the function body** (the approach in our reference
   `Flipper-Zero-ESP32-Port`) — overwrite the prologue with
   `movi.n a2,0 ; retw.n`. We implemented this and it **broke the link** on our
   toolchain (esp-14.2.0 / IDF v5.5.4):
   ```
   dangerous relocation: cannot decode instruction opcode:
     (.text.ieee80211_raw_frame_sanity_check+0x49)
   ```
   The function body carries many `R_XTENSA_SLOT0_OP` relocations. Truncating it
   desyncs the Xtensa instruction stream the linker must decode during
   relaxation. Byte-patching is also fragile: it depends on the exact prologue
   bytes of one blob build.
3. **Weaken the blob's symbol, then override it** — chosen.

## Decision

At CMake **configure time** for the `skyrizz-e32` target, run
`tools/patch_wifi_lib.py` which uses `objcopy --weaken-symbol` to mark
`ieee80211_raw_frame_sanity_check` **WEAK** in
`$IDF_PATH/components/esp_wifi/lib/esp32s3/libnet80211.a` (inside
`ieee80211_output.o`).

We then provide a strong override in
`firmware/platforms/esp32/src/esp32_wifi_raw_override.c`:

```c
int ieee80211_raw_frame_sanity_check(int32_t a, int32_t b, int32_t c) {
    return 0;  // ESP_OK — accept every frame type, incl. 0xC0 / 0xA0
}
```

A strong symbol overrides a weak one with no multiple-definition error. Because
`esp_wifi_80211_tx()` references the check **by symbol** (verified: its
relocation names `ieee80211_raw_frame_sanity_check`, and the symbol is
`FUNC GLOBAL`), its call binds to our override.

Properties:

- **No instruction bytes touched** — no dangling relocations, no decode errors.
- **Version-robust** — depends only on the symbol *name* and its GLOBAL binding,
  not on a byte pattern, so a toolchain/IDF bump does not silently break it.
- **Idempotent** — re-running detects an already-weak symbol and does nothing.
- **Backed up** — the pristine library is copied to `libnet80211.a.orig` on the
  first run; restore by copying it back.
- **Fails loud** — if the toolchain or library is missing, CMake emits a WARNING
  rather than silently shipping a non-functional deauth.

Which frame types this affects: only deauth (`0xC0`) and disassoc (`0xA0`) were
ever blocked. Beacon (`0x80`), probe request (`0x40`), probe response (`0x50`)
and null-data (`0x48`) frames were always allowed — so beacon spam, probe flood,
karma, badmsg and sleep attacks did **not** need this and already transmitted.

## Consequences

- **Deauth Flood works** after the weaken + override + a clean rebuild.
- The weaken step modifies a file **inside the shared ESP-IDF installation**, not
  the project tree. IDF links its own prebuilt blob and offers no override path,
  so this is unavoidable. Consequences:
  - Reinstalling/updating ESP-IDF reverts it — the next build re-applies it.
  - Other projects sharing the same IDF install see the symbol as weak; this is
    harmless unless they *also* define the symbol (then theirs wins, as intended
    for any override). The `.orig` backup documents the original and allows
    manual restore.
- This capability is dual-use. It is gated behind the existing `net.wifi.inject`
  lease + permission flow (ADR 0008); weakening the symbol only removes a
  firmware-level frame-type filter, it does not change the permission model.

## References

- ADR 0008 — hybrid raw radio access via lease + permission.
- `refs/Flipper-Zero-ESP32-Port/tools/patch_wifi_lib.py` — origin of the
  byte-patch technique we rejected in favour of symbol weakening.
- `firmware/targets/skyrizz-e32/tools/patch_wifi_lib.py` — our weaken step.
- `firmware/platforms/esp32/src/esp32_wifi_raw_override.c` — the strong override.
