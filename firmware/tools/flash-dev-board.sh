#!/usr/bin/env bash
# Flash + monitor Dev Board. Usage: flash-dev-board.sh [PORT]
# Console: UART0 (GPIO43/44). Default port auto-detected.
set -euo pipefail

IDF="${IDF_PATH:-$HOME/esp/esp-idf}"
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../targets/dev-board"

PORT="${1:-}"
if [[ -n "$PORT" ]]; then
  idf.py -p "$PORT" flash monitor
else
  idf.py flash monitor
fi
