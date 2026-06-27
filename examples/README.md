# Examples

Kumpulan contoh app untuk Palanu. Tiap folder adalah source `.papp` — tinggal build & install.

Contoh dikelompokkan ke **subfolder kategori** (`ui/`, `network/`, `system/`,
`web3/`). Pengelompokan ini **dinamis**: tooling memindai `examples/` secara
**rekursif** (folder mana pun yang punya `manifest.json` = satu app, di kedalaman
berapa pun), dan struktur kategori dipertahankan saat deploy ke
`/sd/apps/<kategori>/`. Boleh juga taruh app langsung di root `examples/` tanpa
kategori (mis. `hbd/`). Tambah/rename kategori = bikin/rename folder; tidak perlu
ubah script.

## Daftar contoh

| Folder | Runtime | Mode | Deskripsi |
|--------|---------|------|-----------|
| `ui/counter/` | JS | UI | Counter dengan persist storage via `nema.storage.fs` |
| `ui/counter-wasm/` | WASM | UI | Counter versi C — contoh WASM paling minimal |
| `ui/canvas-demo-wasm/` | WASM | UI | Demo menggambar langsung ke canvas |
| `system/sysinfo/` | JS | UI | Device name + capabilities list + tap counter |
| `network/wifi-marauder/` | WASM | UI | WiFi audit: scan, deauth, beacon spam, monitor, evil portal (radio-takeover) |
| `network/network-tools/` | WASM | UI | Utilitas LAN via STA: status koneksi, ARP scan, port scan gateway |
| `web3/web3-test/` | JS | UI | Web3: pick network, tampilkan address (uji wallet/SE) |
| `hbd/` | WASM | Canvas | Happy Birthday — scene bertahap: floor muncul → kue jatuh ke floor → lilin nyala (flicker) → "HAPPY BIRTHDAY!!!" naik dari kue + kedip → prompt + tombol **Blow**. **OK** tiup → asap + 3 balon naik + "Make a wish!". **OK** lagi → finale sparkles + fireworks. **Back** keluar. **+ melodi "Happy Birthday" (tone bit) loop** via `audio_play_tone` |

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
bun run palanu cp "examples/network/wifi-marauder/dist/WiFi Marauder.papp.zip" device:usb3:/sd/apps/network/
```

Atau upload `.papp.zip` via Forge web.

Untuk build **+ deploy semua** sekaligus (memindai rekursif, build, lalu push tiap
app ke `/sd/apps/<kategori>/` sesuai foldernya):

```bash
bun run install-all-examples-apps
```

## Icon

Simpan `icon.png` (32×32) di folder source — SDK otomatis convert ke `icon.raw`
(RGB565) saat build. Field `icon` opsional.
