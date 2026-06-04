# 11 — MVP Integration & Run

> Stage penutup: satukan semua, sediakan satu perintah `bun run sim` (build C++ → start web), tulis run guide, dan validasi kriteria MVP end-to-end.

- Status: ☐ Not started
- Milestone: M1+M2+M3 (penutup MVP)
- Depends on: 01–10 (semua)
- Blocks: — (selesai = MVP)

---

## Goal

- Satu alur "one command" untuk developer: build firmware lalu jalankan simulator web.
- Run guide (README) di root & `packages/simulator`.
- Checklist akhir MVP yang membuktikan **core C++ boot & berjalan penuh di simulator dengan dummy driver, terlihat lewat 4 panel web**.

## Scope

### In scope

- Root script orkestrasi (`bun run sim`): build firmware (CMake) → start Bun server.
- Dokumentasi: prasyarat (cmake/clang/bun), cara jalan, troubleshooting.
- Smoke test end-to-end (manual checklist + opsional script).
- Bersih-bersih: hapus sisa stub/batas-waktu sementara, pastикan mode human & JSON dua-duanya jalan.

### Out of scope

- CI/CD, packaging rilis, ESP32 — semua non-MVP.

---

## Design

### Orkestrasi (root `package.json`)

```jsonc
{
  "scripts": {
    "build:firmware": "bash firmware/tools/build-sim.sh",
    "sim": "bun run build:firmware && bun --cwd packages/simulator run dev",
    "sim:web": "bun --cwd packages/simulator run dev",
    "sim:cli": "bash firmware/tools/run-sim.sh"
  }
}
```

> `bun run sim` = build C++ lalu start web (yang nanti spawn binary saat klik Boot). `sim:cli` = jalankan binary langsung di terminal (mode human-readable) untuk debug cepat tanpa web.

### Run guide (ditulis ke README root + packages/simulator/README.md)

Isi minimal:

1. **Prasyarat**: `cmake`, toolchain clang (`xcode-select --install`), `bun`.
2. **Build & run**:
   ```bash
   bun install
   bun run sim          # build firmware + start simulator web
   # buka URL yang tercetak, klik "Boot"
   ```
3. **Mode CLI (tanpa web)**:
   ```bash
   bun run sim:cli      # output log human-readable
   # atau JSON: KAIRO_SIM_JSON=1 firmware/build/targets/simulator/kairo-sim
   ```
4. **Troubleshooting**: binary tak ketemu → `bun run build:firmware`; port bentrok; cmake belum terpasang.

### Diagram akhir (referensi)

```text
bun run sim
  └─ build kairo-sim (CMake/clang)
  └─ Bun.serve (packages/simulator)
       └─ browser klik Boot → spawn kairo-sim (JSON mode)
            stdout JSON-lines ─► Bun ─► WS ─► panels (Logs/Events/Services)
            panels (Controls) ─► WS ─► Bun ─► stdin ─► kairo-sim
```

---

## Tasks

- [ ] Tambah script `sim`, `sim:web`, `sim:cli` di root `package.json`.
- [ ] Tulis run guide di README root + `packages/simulator/README.md`.
- [ ] Hapus stub/batas-waktu sementara (cek stage 02/07); pastikan shutdown hanya via command/EOF.
- [ ] Verifikasi mode human (`sim:cli`) **dan** mode web end-to-end.
- [ ] (Opsional) smoke test script: spawn binary JSON-mode, kirim beberapa command, assert ada baris `type:ready` & `service Running`.
- [ ] Update `docs/plans/00-overview.md` → centang semua stage selesai.

## Acceptance criteria (Definition of Done — MVP)

- [ ] `bun run sim` dari fresh clone (setelah `bun install`) berhasil build & start web.
- [ ] Klik **Boot** → boot flow penuh: `SystemBoot → … → SystemReady`.
- [ ] **Services panel**: minimal `SimBatteryDriver`, `SimWifiDriver`, `ClockService` = **Running**.
- [ ] **Logs panel**: log terformat mengalir; filter level bekerja.
- [ ] **Events panel**: `SystemReady`, `ClockTick` (~1/s), `BatteryChanged` (periodik) muncul.
- [ ] **Controls panel**: Inject Event muncul kembali di Events; Shutdown → services Stopped & proses keluar; Restart → re-spawn & stream lanjut.
- [ ] Mode CLI human-readable juga jalan (`bun run sim:cli`).
- [ ] `firmware/core/**` tetap hardware-agnostic (tidak ada include platform/OS spesifik; tidak ada `isEsp32()`-style; tidak ada `printf` di luar sink).

## How to verify (skenario end-to-end)

```bash
# 1. fresh
bun install
# 2. build + web
bun run sim
# 3. di browser: Boot → cek Logs/Events/Services → Inject Event → Restart → Shutdown
# 4. mode CLI cepat
bun run sim:cli
```

## Risks / notes

- Ini gerbang mutu MVP; jangan tandai selesai sampai SEMUA acceptance criteria di atas tercentang nyata (bukan asumsi).
- Setelah MVP hijau, kandidat stage berikut (post-MVP): Plugin Runtime (M4), Display + UI Runtime (M5), platform ESP32 + Kairo Dev Board ESP32-S3+e-ink (M6). Lihat overview §0 untuk tier hardware.
```
