#pragma once
#include "nema/hal/driver.h"
#include <cstdint>
#include <cstddef>

namespace nema {

// IOtaUpdater (Plan 39) — firmware OTA writer, TRANSPORT-AGNOSTIC. Any source
// feeds the same sink: a PLP push over USB/BLE (RemoteService Ota channel) or a
// WiFi/HTTP pull all call begin() → write()×N → commit(). The core never touches
// esp_ota directly; the platform provides the implementation (ESP32: esp_ota_*).
// WASM/host have no firmware slots, so supported() is false there.
//
// Flow: begin(totalSize) opens the inactive app slot; write(chunk) appends; commit()
// verifies the image + sets it as next-boot (caller then reboots). On the next
// boot the new image must be confirmed (confirmBoot) or the bootloader rolls back.
struct IOtaUpdater : IDriver {
    const char* name() const override { return "OtaUpdater"; }
    DriverKind  kind() const override { return DriverKind::Other; }

    virtual bool     supported() const = 0;            // false where there's no OTA (WASM)
    virtual bool     begin(uint32_t totalSize) = 0;    // open inactive slot (0 = unknown size)
    virtual bool     write(const uint8_t* data, size_t len) = 0;  // append a chunk
    virtual bool     commit() = 0;                     // verify + set next-boot
    virtual void     abort() = 0;                      // discard the in-progress write
    virtual uint32_t written() const = 0;              // bytes written so far (progress)
    virtual const char* runningSlot() const { return "?"; }  // active partition label

    // Whether commit() should be followed by a device reboot into the new slot.
    // Real hardware: yes. The WASM dry-run: NO — a "restart" there just halts the
    // in-browser device (no auto-reload), which would wedge the session.
    virtual bool rebootOnCommit() const { return true; }

    // Mark the freshly-booted image valid so the bootloader stops watching for a
    // rollback. No-op if no rollback is pending.
    virtual void confirmBoot() {}
};

} // namespace nema
