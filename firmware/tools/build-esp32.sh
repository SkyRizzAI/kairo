#!/usr/bin/env bash
# Build the nema Dev Board firmware (ESP32-S3 + e-ink).
# Sources ESP-IDF, sets target on first run, then builds.
set -euo pipefail

IDF="${IDF_PATH:-$HOME/esp/esp-idf}"
if [[ ! -f "$IDF/export.sh" ]]; then
  echo "ESP-IDF not found at $IDF. Set IDF_PATH or install at ~/esp/esp-idf."
  exit 1
fi
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../targets/dev-board"

# Set target on first configure (idempotent: skips if already esp32s3)
if [[ ! -f sdkconfig ]]; then
  idf.py set-target esp32s3
fi

idf.py build
echo "Build OK → $(pwd)/build/nema-dev-board.bin"
