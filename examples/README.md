# Examples

Kumpulan contoh app untuk Palanu. Tiap folder adalah source `.papp` — tinggal build & install.

## Daftar contoh

| Folder | Runtime | Mode | Deskripsi |
|--------|---------|------|-----------|
| `counter/` | JS | UI | Counter dengan persist storage via `nema.storage.fs` |
| `counter-wasm/` | WASM | UI | Counter versi C — contoh WASM paling minimal |
| `sysinfo/` | JS | UI | Device name + capabilities list + tap counter |
| `canvas-demo-wasm/` | WASM | UI | Demo menggambar langsung ke canvas |
| `wifi-marauder/` | WASM | UI | WiFi audit: scan, deauth, beacon spam, monitor, evil portal (radio-takeover) |
| `network-tools/` | WASM | UI | Utilitas LAN via STA: status koneksi, ARP scan, port scan gateway |

## Build

```bash
# satu app:
bun run app:build:wifi-marauder

# semua:
bun run app:build:all        # JS apps
bun run app:build:all-wasm   # WASM apps
```

Output ada di `examples/<app>/dist/<Nama App>.papp.zip` — dinamai sesuai **nama**
app (mis. `WiFi Marauder.papp.zip`), bukan reverse-DNS id. Manifest `id` cukup
slug pendek (mis. `wifi-marauder`).

## Install ke device

```bash
# tutup serial monitor dulu, lalu (pakai tanda kutip karena ada spasi):
bun run palanu cp "examples/wifi-marauder/dist/WiFi Marauder.papp.zip" device:usb3:/sd/apps/
```

Atau upload `.papp.zip` via Forge web.

## Icon

Simpan `icon.png` (32×32) di folder source — SDK otomatis convert ke `icon.raw`
(RGB565) saat build. Field `icon` opsional.
