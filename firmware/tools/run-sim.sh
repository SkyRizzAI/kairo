#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$(dirname "$SCRIPT_DIR")/build/targets/simulator/kairo-sim"

if [[ ! -f "$BIN" ]]; then
  echo "Binary not found: $BIN"
  echo "Run: bun run build:firmware"
  exit 1
fi

exec "$BIN" "$@"
