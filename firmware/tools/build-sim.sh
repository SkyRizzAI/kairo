#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIRMWARE_DIR="$(dirname "$SCRIPT_DIR")"

cmake -S "$FIRMWARE_DIR" -B "$FIRMWARE_DIR/build" -G "Unix Makefiles"
cmake --build "$FIRMWARE_DIR/build" --target kairo-sim -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"
echo "Build OK → $FIRMWARE_DIR/build/targets/simulator/kairo-sim"
