#!/usr/bin/env bash
# Build SkyRizz E32 audiotest bring-up target.
set -euo pipefail
IDF="${IDF_PATH:-$HOME/esp/esp-idf}"
if [[ ! -f "$IDF/export.sh" ]]; then echo "ESP-IDF not found at $IDF"; exit 1; fi
# shellcheck disable=SC1091
source "$IDF/export.sh" >/dev/null
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../targets/skyrizz-audiotest"
if [[ ! -f sdkconfig ]]; then idf.py set-target esp32s3; fi
idf.py build
echo "Build OK → $(pwd)/build/kairo-skyrizz-audiotest.bin"
