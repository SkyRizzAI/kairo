# Examples

Kumpulan contoh app untuk Palanu. Tiap folder adalah source `.papp` — tinggal build & install.

## Build

```bash
# Build dari repo root:
bun run app:build:hello
bun run app:build:deauth

# Atau dari folder example langsung:
cd examples/hello
bun run ../../packages/app-sdk/bin/nema-build.ts
```

Output: `dist/<id>.papp/` — folder siap copy ke `/apps/` di device.

## Install

Copy folder `dist/<id>.papp/` ke `/apps/` di device via Forge FileBrowser atau CLI.

## Daftar contoh

| App | Deskripsi | Runtime |
|-----|-----------|---------|
| `hello` | Counter + device info + persist storage | JS |
| `deauth` | WiFi tools demo | JS |
