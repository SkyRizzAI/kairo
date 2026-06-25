# 13 — Override `btInUse()` so arduino-esp32 doesn't free the BLE controller memory

- Status: Accepted
- Date: 2026-06-25
- Related: Plan 93 (BLE bring-up), ADR 0010 (internal-SRAM budget)

## Context

On `skyrizz-e32` (ESP32-S3, arduino-esp32 as an ESP-IDF managed component), enabling
Bluetooth **crashed the device deterministically** — a `LoadProhibited` panic inside the
closed-source BLE controller blob:

```
esp_bt_controller_init → btdm_controller_init → btdm_controller_deinit_internal
  → semphr_delete_wrapper(0x12)   // garbage semaphore handle
```

The crash was **independent of everything we tried**: it reproduced with 167 KB free internal
RAM and a 90 KB contiguous block (so not memory/fragmentation), with WiFi off and BLE
initialised first (not coexistence/ordering), with BLE 5.0 features disabled (4.2-only, matching
the known-good `refs/Flipper-Zero-ESP32-Port`), and with SW coexistence disabled. We chased all
of those dead ends.

Root cause: **arduino-esp32's `initArduino()` calls
`esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)` at boot** to reclaim RAM when it thinks BT is
unused — guarded by `if (!btInUse())` (`cores/esp32/esp32-hal-misc.c`). `btInUse()` is a **weak
symbol defaulting to `false`** (`cores/esp32/esp32-hal-bt.c`), and arduino explicitly documents:
*"Users can also provide their own strong btInUse() implementation."* Because kairo drives
NimBLE through ESP-IDF directly (not arduino's BLE classes), nothing claimed BT, so the
controller memory was freed at boot. Our later `esp_bt_controller_init()` then ran on released
memory → the controller failed init and its own rollback dereferenced uninitialised semaphore
handles → panic.

## Decision

Provide a **strong `btInUse()` returning `true`** in the ESP32 BLE driver, guarded by
`CONFIG_BT_NIMBLE_ENABLED`:

```cpp
// firmware/platforms/esp32/src/esp32_ble.cpp (NimBLE section)
extern "C" bool btInUse(void) { return true; }
```

This overrides arduino's weak symbol so `initArduino()` skips the controller mem-release and the
BLE controller memory stays intact for `esp_bt_controller_init()`.

Consequence-driven follow-on: the BLE controller is initialised **on demand** (first `enable()`),
**not at boot**. Once BLE successfully initialises it takes ~30 KB of internal SRAM (controller
task stacks, semaphores, mbuf pools). On this internal-RAM-tight board (see ADR 0010), taking
that at boot starves the microSD DMA bounce buffer (`sdmmc_read_sectors: not enough mem`), which
makes apps loaded from `/sd` get dropped from the registry. Deferring the controller init to the
first toggle keeps boot clean and SD/apps intact.

## Consequences

- **BLE works.** Toggling Bluetooth brings the controller up and advertises without crashing.
- **Any board/target that uses NimBLE via ESP-IDF under arduino-esp32 must define `btInUse()`** —
  this is the non-obvious prerequisite. Without it the controller memory is gone before init.
- BLE remains **on-demand**: ~30 KB internal SRAM is only consumed while Bluetooth is enabled,
  preserving the SD/app/mDNS internal-RAM budget at boot. Enabling BLE while doing heavy
  concurrent SD I/O can still be tight; the `enable()` path keeps a heap pre-flight guard that
  refuses (Bluetooth "unavailable") rather than risk an allocation failure.
- The earlier "init at boot to dodge fragmentation" theory is **withdrawn** — fragmentation was
  never the cause. Several RAM-diet tweaks made during the hunt (chunkbuf rows, WiFi static RX)
  are retained only as headroom and may be restored to their originals.
- Diagnostic lesson recorded: a crash *inside a closed controller blob's deinit on a garbage
  handle* points to the controller memory having been released/clobbered before init — check
  `esp_bt_controller_mem_release` callers (arduino's `btInUse`) before suspecting RAM or config.
