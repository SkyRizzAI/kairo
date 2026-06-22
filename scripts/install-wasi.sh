#!/usr/bin/env bash
set -e

WASI_VERSION=24
WASI_VERSION_FULL=${WASI_VERSION}.0

# --- Auto-detect OS ---
case "$(uname -s)" in
  Linux*)  WASI_OS=linux ;;
  Darwin*) WASI_OS=macos ;;
  *) echo "OS tidak didukung"; exit 1 ;;
esac

# --- Auto-detect arch ---
case "$(uname -m)" in
  x86_64)         WASI_ARCH=x86_64 ;;
  arm64|aarch64)  WASI_ARCH=arm64 ;;   # M-series Mac / ARM Linux
  *) echo "Arch tidak didukung"; exit 1 ;;
esac

# Catatan: di Linux release lama (<25) nama folder cuma "wasi-sdk-24.0" tanpa arch-os.
# Mulai rilis baru semua platform formatnya seragam: wasi-sdk-<ver>-<arch>-<os>
PKG=wasi-sdk-${WASI_VERSION_FULL}-${WASI_ARCH}-${WASI_OS}

# --- Download (curl jalan di Linux & macOS; wget gak ada di macOS bawaan) ---
curl -L https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_VERSION}/${PKG}.tar.gz -o "${PKG}.tar.gz"

# --- Extract ke /opt + symlink ---
sudo tar xvf "${PKG}.tar.gz" -C /opt
sudo ln -sf /opt/${PKG} /opt/wasi-sdk

echo "Done. Set: export WASI_SDK_PATH=/opt/wasi-sdk"
