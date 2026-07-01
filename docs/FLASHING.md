# Flashing Palanu firmware

Every release ships **two files per board**, named so you never have to guess:

| File | Use it when | How |
|---|---|---|
| `palanu-<board>-<ver>-**factory**.bin` | **First flash / blank board / un-brick.** Full image (bootloader + partition table + app + assets). | Cable, write to **offset `0x0`**. |
| `palanu-<board>-<ver>-**ota**.bin` | **Updating a device that already runs Palanu.** App only. | Over-the-air (Forge), or cable to the app slot. |

Each `.bin` has a `.sha256` sidecar for integrity. `<board>` is e.g. `skyrizz-e32`
or `skyrizz-solana`.

> ⚠️ **Do not flash `-ota.bin` to `0x0`.** It's only the app, not a bootable image —
> the chip will loop with `Invalid image block, can't boot`. For a blank board you
> want `-factory.bin`. Conversely, never push `-factory.bin` over OTA — it's far
> larger than the A/B slot.

---

## 1. Factory / first flash (cable)

The device must be in **download mode** (ESP32-S3): power off, hold **BOOT**, power on,
release **BOOT**. Then, from a machine with [esptool](https://github.com/espressif/esptool):

```bash
# optional but recommended for a fresh/bricked board
esptool.py --chip esp32s3 -p <PORT> erase_flash

# write the full image to 0x0
esptool.py --chip esp32s3 -p <PORT> -b 460800 \
  write_flash 0x0 palanu-<board>-<ver>-factory.bin
```

`<PORT>` = `/dev/cu.usbmodem*` (macOS) · `/dev/ttyACM0`/`ttyUSB0` (Linux) · `COMx` (Windows).

**No esptool?** Use the browser flasher: open Forge → **Flash** (`/flash`), pick the
board, hit *Flash device* (Chrome/Edge/Opera, Web Serial — fully client-side).

If it still won't boot after a correct factory flash, drop the flash frequency (a
marginal board / long USB cable):

```bash
esptool.py --chip esp32s3 -p <PORT> write_flash --flash_freq 40m 0x0 palanu-<board>-<ver>-factory.bin
```

…and confirm the module is really the expected part:

```bash
esptool.py --chip esp32s3 -p <PORT> flash_id   # expect 16 MB; ESP32-S3
```

## 2. OTA update (no cable)

In Forge → **Firmware**, pick the release; the panel lists only `-ota.bin` builds.
It streams the app to the **inactive A/B slot** and reboots into it — the bootloader
rolls back automatically if the new image fails to confirm a good boot. The device
must already be running Palanu (paired over USB/BLE/Wi-Fi).

## 3. Verify a download

```bash
sha256sum -c palanu-<board>-<ver>-factory.bin.sha256   # Linux
shasum -a 256 -c palanu-<board>-<ver>-factory.bin.sha256  # macOS
```

---

*Producing these locally:* `firmware/tools/package-firmware.sh <board>` (after
sourcing ESP-IDF) writes both files to `firmware/targets/<board>/build/dist/`. For the
browser `/flash` registry, `firmware/tools/publish-firmware.sh` stages parts into
Forge's static dir. Offsets are always read from the build's `flasher_args.json`.
