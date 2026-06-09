#!/usr/bin/env bash
# Build the Kairo firmware to WebAssembly (Emscripten) and copy the artifacts
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
cmake --build "$BUILD_DIR" --target kairo -j"$(sysctl -n hw.logicalcpu 2>/dev/null || nproc)"

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR/kairo.mjs"
cp "$BUILD_DIR/targets/wasm/kairo.js"   "$OUT_DIR/"
cp "$BUILD_DIR/targets/wasm/kairo.wasm" "$OUT_DIR/"

echo "WASM OK → $OUT_DIR (kairo.js + kairo.wasm)"
