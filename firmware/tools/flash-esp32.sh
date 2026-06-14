#!/usr/bin/env bash
# Flash + monitor the Palanu Dev Board. Usage: flash-esp32.sh [PORT]
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
  idf.py flash monitor   # auto-detect port
fi
