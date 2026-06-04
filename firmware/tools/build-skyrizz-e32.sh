#!/usr/bin/env bash
# Build Kairo firmware for SkyRizz E32 (ESP32-S3-N16R8 + TFT LCD + XL9535 3-button).
set -euo pipefail

IDF="${IDF_PATH:-$HOME/esp/esp-idf}"
if [[ ! -f "$IDF/export.sh" ]]; then
  echo "ESP-IDF not found at $IDF. Set IDF_PATH or install at ~/esp/esp-idf."
  exit 1
fi
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../targets/skyrizz-e32"

if [[ ! -f sdkconfig ]]; then
  idf.py set-target esp32s3
fi

idf.py build
echo "Build OK → $(pwd)/build/kairo-skyrizz-e32.bin"
