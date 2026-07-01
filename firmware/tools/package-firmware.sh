#!/usr/bin/env bash
# Build an ESP32 target and package the two release artifacts, named so their
# purpose is unambiguous from the filename alone:
#
#   dist/palanu-<board>-<ver>-factory.bin   → CABLE flash to offset 0x0 (blank board).
#                                             Full image: bootloader + partition table
#                                             + otadata + app + spiffs, merged.
#   dist/palanu-<board>-<ver>-ota.bin       → OTA update (app-only). Streamed to the
#                                             inactive A/B slot by the updater; the
#                                             bootloader/partition table are untouched.
#
# Each .bin gets a .sha256 sidecar. Offsets + flash params come from the build's
# flasher_args.json — never hard-coded, so they can't drift from partitions.csv.
#
# Requires the ESP-IDF env already sourced (idf.py + esptool.py on PATH).
# Usage: firmware/tools/package-firmware.sh <board>        e.g. skyrizz-solana
set -euo pipefail

BOARD="${1:?usage: package-firmware.sh <board>  (e.g. skyrizz-e32 | skyrizz-solana)}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TARGET="$ROOT/firmware/targets/$BOARD"
[ -d "$TARGET" ] || { echo "unknown board: $BOARD (no firmware/targets/$BOARD)"; exit 1; }

cd "$TARGET"
[ -f sdkconfig ] || idf.py set-target esp32s3
idf.py build

BUILD="$TARGET/build"
APP="$BUILD/palanu-$BOARD.bin"
[ -f "$APP" ] || { echo "app image missing: $APP (build failed?)"; exit 1; }

# Version for the filename: an explicit FW_VERSION (CI passes the release tag) wins;
# else git describe. Strip a leading "palanu-" so we don't get palanu-<board>-palanu-v…
VER="${FW_VERSION:-$(git -C "$ROOT" describe --tags --always --dirty 2>/dev/null || echo dev)}"
VER="${VER#palanu-}"
DIST="$BUILD/dist"
rm -rf "$DIST"; mkdir -p "$DIST"

FACTORY="$DIST/palanu-$BOARD-$VER-factory.bin"
OTA="$DIST/palanu-$BOARD-$VER-ota.bin"

# factory: single full-flash image (→ 0x0). Offsets read from flasher_args.json.
( cd "$BUILD" && esptool.py --chip esp32s3 merge_bin -o "$FACTORY" @flash_args )
# ota: the plain app image, exactly what the OTA updater writes to the A/B slot.
cp "$APP" "$OTA"

# checksums (portable across Linux/macOS)
_sha() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi
}
( cd "$DIST" && for f in *.bin; do _sha "$f" > "$f.sha256"; done )

echo "packaged $BOARD ($VER):"
ls -la "$DIST"
