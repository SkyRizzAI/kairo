#!/usr/bin/env bash
# Local DEV build for OTA testing. Bumps a gitignored counter so every build has a
# distinct version (so you can confirm an OTA actually swapped the image via the
# `version` CLI), builds the target, and prints the .bin to upload through Forge.
#
# Does NOT touch any tracked file — the counter lives in firmware/.dev-build
# (gitignored), and the version suffix is injected only via -DNEMA_DEV_BUILD.
#
# Usage:  bash tools/build-dev-ota.sh [skyrizz-e32|dev-board]   (default skyrizz-e32)
set -euo pipefail

TARGET="${1:-skyrizz-e32}"
case "$TARGET" in
  skyrizz-e32|dev-board) ;;
  *) echo "unknown target '$TARGET' (use: skyrizz-e32 | dev-board)"; exit 1 ;;
esac

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FW_DIR="$(dirname "$SCRIPT_DIR")"
COUNTER="$FW_DIR/.dev-build"            # gitignored — never committed

N=$(( $(cat "$COUNTER" 2>/dev/null || echo 0) + 1 ))
echo "$N" > "$COUNTER"

IDF="${IDF_PATH:-$HOME/esp/esp-idf}"
if [[ ! -f "$IDF/export.sh" ]]; then
  echo "ESP-IDF not found at $IDF. Set IDF_PATH or install at ~/esp/esp-idf."
  exit 1
fi
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null

cd "$FW_DIR/targets/$TARGET"
[[ -f sdkconfig ]] || idf.py set-target esp32s3
idf.py -DNEMA_DEV_BUILD="$N" build

BIN="$(pwd)/build/palanu-$TARGET.bin"
echo ""
echo "================ DEV OTA build #$N ($TARGET) ================"
echo "image: $BIN"
echo ""
echo "First time on this device (adopts the A/B layout) — flash over CABLE once:"
echo "    cd $(pwd) && idf.py -p <PORT> flash"
echo ""
echo "After that, update OVER THE AIR:"
echo "    Forge → /remote → connect (USB/BLE) → Update firmware → choose the .bin above → Push update"
echo ""
echo "Confirm it took: run  version  in the CLI — it should report  ...dev$N"
