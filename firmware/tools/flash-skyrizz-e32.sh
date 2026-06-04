#!/usr/bin/env bash
# Flash + monitor SkyRizz E32. Usage: flash-skyrizz-e32.sh [PORT]
# Console: USB CDC (/dev/ttyACM0 or similar) — UART0 GPIO43/44 occupied by XL9535+SPI.
set -euo pipefail

IDF="${IDF_PATH:-$HOME/esp/esp-idf}"
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../targets/skyrizz-e32"

PORT="${1:-}"
if [[ -n "$PORT" ]]; then
  idf.py -p "$PORT" flash monitor
else
  # USB CDC port biasanya /dev/ttyACM0 (Linux) atau /dev/cu.usbmodem* (macOS)
  idf.py flash monitor
fi
