#!/usr/bin/env bash
# Stage built ESP32 firmware into Forge's static dir + write the manifest the web
# flasher (esptool-js, Web Serial at /flash) and the OTA registry (tRPC firmware.*)
# read. Run AFTER `idf.py build` in each target. The binaries are plain static
# assets; the manifest is a single JSON file.
#
# Parts (offsets + flash params) are derived from each build's flasher_args.json —
# NEVER hard-coded — so they always match partitions.csv (the old fixed 0x10000 app
# offset silently disagreed with our 0x20000 A/B layout and would brick the flash).
#
# Usage: firmware/tools/publish-firmware.sh [target ...]   (default: both skyrizz boards)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/packages/forge/static/firmware"
TARGETS=("$@")
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(skyrizz-e32 skyrizz-solana)
mkdir -p "$OUT"
VERSION="$(git -C "$ROOT" describe --tags --always --dirty 2>/dev/null || echo dev)"

ROOT="$ROOT" OUT="$OUT" VERSION="$VERSION" TARGETS="${TARGETS[*]}" python3 - <<'PY'
import json, os, shutil, sys

root    = os.environ["ROOT"]
out     = os.environ["OUT"]
version = os.environ["VERSION"]
targets = os.environ["TARGETS"].split()

builds = []
for t in targets:
    build = os.path.join(root, "firmware/targets", t, "build")
    fa    = os.path.join(build, "flasher_args.json")
    if not os.path.isfile(fa):
        print(f"skip {t} — no flasher_args.json (run `idf.py build` first)", file=sys.stderr)
        continue
    fj       = json.load(open(fa))
    settings = fj["flash_settings"]
    app_file = fj["app"]["file"]                     # e.g. palanu-skyrizz-solana.bin
    chip     = fj.get("extra_esptool_args", {}).get("chip", "esp32s3")

    dst = os.path.join(out, t)
    os.makedirs(dst, exist_ok=True)
    parts, appsize, ok = [], 0, True
    # flash_files maps {offset: relpath}; copy each part, keep correct offsets.
    for off, rel in sorted(fj["flash_files"].items(), key=lambda kv: int(kv[0], 16)):
        src = os.path.join(build, rel)
        if not os.path.isfile(src):
            print(f"skip {t} — missing part {rel}", file=sys.stderr); ok = False; break
        base = os.path.basename(rel)
        shutil.copy(src, os.path.join(dst, base))
        parts.append({"offset": off, "path": f"/firmware/{t}/{base}"})
        if rel == app_file:
            appsize = os.path.getsize(src)
    if not ok:
        continue

    builds.append({
        "id": t, "board": t, "chip": chip, "version": version, "appSize": appsize,
        "flash": {"mode": settings["flash_mode"], "freq": settings["flash_freq"],
                  "size": settings["flash_size"]},
        "parts": parts,
    })
    print(f"published {t} ({appsize} B app, {len(parts)} parts, {version})")

if not builds:
    print("no firmware published", file=sys.stderr); sys.exit(1)

json.dump({"version": version, "builds": builds},
          open(os.path.join(out, "manifest.json"), "w"), indent=2)
print("manifest →", os.path.join(out, "manifest.json"))
PY
