#!/usr/bin/env bash
# Flash + monitor SkyRizz Solana. Usage: flash-skyrizz-solana.sh [PORT]
# Console: USB Serial/JTAG (HWCDC) or CH340C UART0 — depends on USB mode.
set -euo pipefail

IDF="${IDF_PATH:-$HOME/esp/esp-idf}"
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../targets/skyrizz-solana"

PORT="${1:-}"
if [[ -n "$PORT" ]]; then
  idf.py -p "$PORT" flash monitor
else
  # macOS: /dev/cu.usbmodem* (USB Serial/JTAG) or /dev/cu.usbserial* (CH340C UART0)
  idf.py flash monitor
fi
