#!/usr/bin/env bash
# Build the Nema firmware to WebAssembly (Emscripten) and copy the artifacts
# into Forge's static dir so the browser can load them. Needs emsdk active:
#   source ~/emsdk/emsdk_env.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIRMWARE_DIR="$(dirname "$SCRIPT_DIR")"
REPO_ROOT="$(dirname "$FIRMWARE_DIR")"
BUILD_DIR="$FIRMWARE_DIR/build-wasm"
OUT_DIR="$REPO_ROOT/packages/forge/static/wasm"

if ! command -v emcc >/dev/null 2>&1; then
    echo "error: emcc not found. Run: source ~/emsdk/emsdk_env.sh" >&2
    exit 1
fi

emcmake cmake -S "$FIRMWARE_DIR" -B "$BUILD_DIR" -G "Unix Makefiles"
cmake --build "$BUILD_DIR" --target palanu -j"$(sysctl -n hw.logicalcpu 2>/dev/null || nproc)"

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR/palanu.mjs"
cp "$BUILD_DIR/targets/wasm/palanu.js"   "$OUT_DIR/"
cp "$BUILD_DIR/targets/wasm/palanu.wasm" "$OUT_DIR/"

echo "WASM OK → $OUT_DIR (palanu.js + palanu.wasm)"
