#!/usr/bin/env bash
# Build nema firmware for SkyRizz Solana ("Lanyard v2":
# ESP32-S3-N16R8 + ILI9341 TFT + TCA9534 6-button D-pad).
set -euo pipefail

IDF="${IDF_PATH:-$HOME/esp/esp-idf}"
if [[ ! -f "$IDF/export.sh" ]]; then
  echo "ESP-IDF not found at $IDF. Set IDF_PATH or install at ~/esp/esp-idf."
  exit 1
fi
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../targets/skyrizz-solana"

if [[ ! -f sdkconfig ]]; then
  idf.py set-target esp32s3
fi

idf.py build
echo "Build OK → $(pwd)/build/palanu-skyrizz-solana.bin"
