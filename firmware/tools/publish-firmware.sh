#!/usr/bin/env bash
# Stage built ESP32 firmware bundles into Forge's static dir + write a manifest
# the web flasher (esptool-js, Web Serial) and the OTA registry (tRPC firmware.*)
# read. Run AFTER `idf.py build` in the target. Serverless-friendly: the binaries
# are plain static assets, the manifest is a single JSON file.
#
# Usage: firmware/tools/publish-firmware.sh [target ...]   (default: skyrizz-e32)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/packages/forge/static/firmware"
TARGETS=("${@:-skyrizz-e32}")

mkdir -p "$OUT"
VERSION="$(git -C "$ROOT" describe --tags --always --dirty 2>/dev/null || echo dev)"
ENTRIES=()

for t in "${TARGETS[@]}"; do
    BUILD="$ROOT/firmware/targets/$t/build"
    APP="$BUILD/nema-$t.bin"
    BOOT="$BUILD/bootloader/bootloader.bin"
    PART="$BUILD/partition_table/partition-table.bin"
    if [[ ! -f "$APP" || ! -f "$BOOT" || ! -f "$PART" ]]; then
        echo "skip $t — build artifacts missing (run idf.py build first)" >&2
        continue
    fi
    dst="$OUT/$t"
    mkdir -p "$dst"
    cp "$BOOT" "$dst/bootloader.bin"
    cp "$PART" "$dst/partition-table.bin"
    cp "$APP"  "$dst/app.bin"
    appsize=$(wc -c < "$APP" | tr -d ' ')

    ENTRIES+=("$(cat <<JSON
    {
      "id": "$t",
      "board": "$t",
      "chip": "esp32s3",
      "version": "$VERSION",
      "appSize": $appsize,
      "flash": { "mode": "dio", "freq": "80m", "size": "16MB" },
      "parts": [
        { "offset": "0x0",     "path": "/firmware/$t/bootloader.bin" },
        { "offset": "0x8000",  "path": "/firmware/$t/partition-table.bin" },
        { "offset": "0x10000", "path": "/firmware/$t/app.bin" }
      ]
    }
JSON
)")
    echo "published $t ($appsize bytes app, version $VERSION)"
done

if [[ ${#ENTRIES[@]} -eq 0 ]]; then
    echo "no firmware published" >&2
    exit 1
fi

# Join entries with commas into the manifest array.
{
    echo '{'
    echo "  \"version\": \"$VERSION\","
    echo '  "builds": ['
    for i in "${!ENTRIES[@]}"; do
        printf '%s' "${ENTRIES[$i]}"
        [[ $i -lt $((${#ENTRIES[@]} - 1)) ]] && echo ',' || echo ''
    done
    echo '  ]'
    echo '}'
} > "$OUT/manifest.json"

echo "manifest → $OUT/manifest.json"
