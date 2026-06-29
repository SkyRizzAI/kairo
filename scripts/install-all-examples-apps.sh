#!/usr/bin/env bash
#
# install-all-examples-apps.sh — build every example app under examples/ and deploy
# each resulting .papp.zip to a Palanu device over palanu (serial/WS).
#
# Flow:
#   1. Scan examples/ RECURSIVELY for apps (any dir with a manifest.json), at any
#      nesting depth — so category subfolders work, e.g.:
#          examples/ui/counter        examples/network/wifi-marauder
#          examples/system/sysinfo    examples/hbd            (root, uncategorised)
#   2. Build them all via the app-sdk builder (JS + WASM).
#   3. Prompt for a device id (palanu alias) — press Enter to skip deploy (build only).
#   3b. Prompt for install location: SD card (/sd/apps) or system (/system/apps).
#   4. palanu cp each dist/*.papp.zip → device:<id>:<approot>/<category>/ (preserving
#      the examples/ subpath; palanu auto-unzips, the device scans the root recursively).
#
# Registered in package.json as "install-all-examples-apps".
# Usage:  bun run install-all-examples-apps

set -o pipefail

# ── Repo root (script lives in scripts/) ────────────────────────────────────
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

BUILD=(bun run packages/app-sdk/bin/build.ts)
TMP="${CLAUDE_JOB_DIR:-/tmp}/tmp"
mkdir -p "$TMP" 2>/dev/null || TMP="/tmp"

# ── 1. Scan (recursive, any depth) ──────────────────────────────────────────
# Each "app" is stored as its path RELATIVE to examples/ (e.g. "ui/counter",
# "hbd"). dist/ and node_modules/ are pruned so build outputs aren't rescanned.
echo "🔍 Scanning examples/ recursively for apps (dirs with a manifest.json)…"
apps=()
while IFS= read -r m; do
  dir="$(dirname "$m")"
  apps+=("${dir#examples/}")            # strip leading "examples/"
done < <(
  find examples \
    \( -type d \( -name dist -o -name node_modules \) -prune \) -o \
    -type f -name manifest.json -print | sort
)

if [ "${#apps[@]}" -eq 0 ]; then
  echo "❌ No example apps found (no examples/**/manifest.json)."
  exit 1
fi
echo "   Found ${#apps[@]}: ${apps[*]}"
echo

# ── 2. Build ────────────────────────────────────────────────────────────────
echo "🔨 Building…"
built=()
for app in "${apps[@]}"; do
  printf "   • %-30s " "$app"
  log="$TMP/build-${app//\//-}.log"      # flatten slashes for the log filename
  if "${BUILD[@]}" --dir "examples/$app" >"$log" 2>&1; then
    echo "✓"
    built+=("$app")
  else
    echo "✗  (log: $log)"
  fi
done
echo "   Built ${#built[@]}/${#apps[@]}."
echo

if [ "${#built[@]}" -eq 0 ]; then
  echo "❌ Nothing built — aborting."
  exit 1
fi

# ── 3. Device prompt ────────────────────────────────────────────────────────
echo "📟 Registered devices:"
bun run palanu list 2>/dev/null | grep -vE '^\s*\$|palanu\.ts' || true
echo
read -rp "   Device id to deploy to (Enter to skip / build-only): " DEVICE
if [ -z "${DEVICE:-}" ]; then
  echo "ℹ️  No device id — built only, skipping deploy."
  exit 0
fi
read -rsp "   Device password (Enter for none): " PW; echo
echo

# ── 3b. Install location ────────────────────────────────────────────────────
# Both are valid app roots the device scans (see kAppRoots). SD card is the usual
# choice (removable, big); system is internal flash (survives a missing SD card).
echo "💾 Install location:"
echo "     1) SD card  →  /sd/apps        (default, removable)"
echo "     2) System   →  /system/apps    (internal flash)"
read -rp "   Choose [1/2, Enter=1]: " LOC
case "${LOC:-1}" in
  2) APPROOT="/system/apps" ;;
  1|"") APPROOT="/sd/apps" ;;
  *) echo "   ‼️  Invalid choice '$LOC' — defaulting to SD card."; APPROOT="/sd/apps" ;;
esac
echo "   → installing under $APPROOT"
echo

# ── 4. Deploy (preserve category subpath) ───────────────────────────────────
echo "🚀 Deploying to device:$DEVICE:$APPROOT/ …"
ok=0; fail=0
for app in "${built[@]}"; do
  zip="$(ls "examples/$app/dist/"*.papp.zip 2>/dev/null | head -1)"
  if [ -z "$zip" ]; then
    printf "   • %-30s ✗  (no .papp.zip)\n" "$app"
    fail=$((fail + 1)); continue
  fi

  # category = the directory part of the relative path ("ui/counter" → "ui",
  # "hbd" → ""). Deploy into <APPROOT>/<category>/ so the on-device recursive
  # scanner keeps the same folder structure.
  category="$(dirname "$app")"
  if [ "$category" = "." ]; then
    dest="device:$DEVICE:$APPROOT/"
  else
    dest="device:$DEVICE:$APPROOT/$category/"
  fi

  printf "   • %-40s " "$category/$(basename "$zip")"
  if [ -n "$PW" ]; then
    bun run palanu cp "$zip" "$dest" --password "$PW" >"$TMP/cp-${app//\//-}.log" 2>&1
  else
    bun run palanu cp "$zip" "$dest" >"$TMP/cp-${app//\//-}.log" 2>&1
  fi
  if [ $? -eq 0 ]; then
    echo "✓"; ok=$((ok + 1))
  else
    echo "✗  (log: $TMP/cp-${app//\//-}.log)"; fail=$((fail + 1))
  fi
done

echo
if [ "$fail" -eq 0 ]; then
  echo "✅ Deployed all $ok app(s) to device:$DEVICE:$APPROOT/ (appScan triggered)."
else
  echo "⚠️  Deployed $ok, $fail failed — check the logs above."
fi
