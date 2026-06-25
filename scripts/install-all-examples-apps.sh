#!/usr/bin/env bash
#
# install-all-examples-apps.sh — build every example app under examples/ and deploy
# each resulting .papp.zip to a Palanu device over palanu (serial/WS).
#
# Flow:
#   1. Scan examples/ for apps (any dir with a manifest.json).
#   2. Build them all via the app-sdk builder (JS + WASM).
#   3. Prompt for a device id (palanu alias) — press Enter to skip deploy (build only).
#   4. palanu cp each dist/*.papp.zip → device:<id>:/sd/apps/ (palanu auto-unzips to .papp).
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

# ── 1. Scan ─────────────────────────────────────────────────────────────────
echo "🔍 Scanning examples/ for apps (dirs with a manifest.json)…"
apps=()
for m in examples/*/manifest.json; do
  [ -e "$m" ] || continue
  apps+=("$(basename "$(dirname "$m")")")
done

if [ "${#apps[@]}" -eq 0 ]; then
  echo "❌ No example apps found (no examples/*/manifest.json)."
  exit 1
fi
echo "   Found ${#apps[@]}: ${apps[*]}"
echo

# ── 2. Build ────────────────────────────────────────────────────────────────
echo "🔨 Building…"
built=()
for app in "${apps[@]}"; do
  printf "   • %-22s " "$app"
  if "${BUILD[@]}" --dir "examples/$app" >"$TMP/build-$app.log" 2>&1; then
    echo "✓"
    built+=("$app")
  else
    echo "✗  (log: $TMP/build-$app.log)"
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

# ── 4. Deploy ───────────────────────────────────────────────────────────────
echo "🚀 Deploying to device:$DEVICE:/sd/apps/ …"
ok=0; fail=0
for app in "${built[@]}"; do
  zip="$(ls "examples/$app/dist/"*.papp.zip 2>/dev/null | head -1)"
  if [ -z "$zip" ]; then
    printf "   • %-22s ✗  (no .papp.zip)\n" "$app"
    fail=$((fail + 1)); continue
  fi
  printf "   • %-30s " "$(basename "$zip")"
  if [ -n "$PW" ]; then
    bun run palanu cp "$zip" "device:$DEVICE:/sd/apps/" --password "$PW" >"$TMP/cp-$app.log" 2>&1
  else
    bun run palanu cp "$zip" "device:$DEVICE:/sd/apps/" >"$TMP/cp-$app.log" 2>&1
  fi
  if [ $? -eq 0 ]; then
    echo "✓"; ok=$((ok + 1))
  else
    echo "✗  (log: $TMP/cp-$app.log)"; fail=$((fail + 1))
  fi
done

echo
if [ "$fail" -eq 0 ]; then
  echo "✅ Deployed all $ok app(s) to device:$DEVICE:/sd/apps/ (appScan triggered)."
else
  echo "⚠️  Deployed $ok, $fail failed — check the logs above."
fi
