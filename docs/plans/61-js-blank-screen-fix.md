# 61 — JS App Blank Screen Bug Fix (SkyRizz E32)

> Investigasi & fix intermittent blank screen pada JS apps di hardware SkyRizz
> E32. Counter & SysInfo kadang muncul, kadang blank. Root cause unknown.

- Status: 🔴 Not started
- Depends on: 28 (SkyRizz E32), 58 (JS Runtime), 60 (Aether UI Rewrite)
- Blocks: P1-1 (NTP), P1-2 (WiFi HW verify), semua UX di HW

---

## 1. Gejala

- `present()` dikonfirmasi jalan: Counter nz=199, SysInfo nz=3301 (ada pixel
  non-zero di framebuffer)
- Tapi tampilan intermittent — kadang render ke display, kadang tidak
- Hello app sudah fixed (stub → ComponentApp)
- Tidak ada crash log atau exception di serial output

## 2. Hipotesis (yang belum dicek)

### A. Race: `draw()` sebelum `present()` pertama

```
AppHost draw n=0 hasFrame=0  ← draw sebelum present
AppHost draw n=1 hasFrame=0
AppHost present n=0 nz=199   ← present setelah draw sudah lewat
AppHost draw n=2 hasFrame=1  ← baru bisa draw
```

`AppHost::present()` sudah panggil `rt_.view().requestRedraw()`. Tapi jika
GuiService loop sudah lewat satu siklus sebelum redraw request diproses,
frame pertama terlewat. Tidak ada trigger redraw berikutnya.

**Cek:** apakah `frameSeq_ != drawnSeq_` safety net di `AppHost::tick()` firing?

### B. DPM sleep/lock masuk sebelum frame pertama

DPM timeout mungkin trigger `GuiService::sleep()` sebelum frame pertama selesai
render. Saat device sleep, display dimatikan — user lihat blank.

**Cek:** timing DPM default vs js app boot time. Apakah js app butuh >5 detik
untuk frame pertama?

### C. SPI / framebuffer corruption

ESP32-S3 SPI ke display bisa race jika DMA channel dishare atau buffer alignment
salah. `flushBuffer` vs `flush` path — apakah ada kondisi di mana data ditulis
ke framebuffer internal tapi tidak benar-benar di-flush ke panel fisik?

**Cek:** `LcdDriver` flush path untuk SkyRizz E32 — apakah ada early return
atau kondisi yang skip SPI transfer?

### D. QuickJS GC pause atau heap exhaustion

QuickJS GC bisa pause 100-500ms pada ESP32-S3. Jika GC jalan pas GuiService
loop, frame bisa terlewat.

**Cek:** QuickJS memory limit, apakah ada allocation yang menjelang batas?

### E. Backlight state (XL9535)

XL9535 GPIO expander kontrol backlight. Jika backlight tidak di-set ON setelah
boot, display secara teknis render frame tapi user lihat blank.

**Cek:** apakah `Board::initDisplay()` atau power-on sequence set backlight ON?

## 3. Rencana Investigasi

### Fase 1 — Instrumentasi (1 hari)

Tambahkan log trace di titik kritis:

```cpp
// GuiService::loop() — setelah renderFrame + canvas.flush
rt.log().trace("GuiService", "frame flushed",
    {{"frameSeq", frameSeq}, {"flushUs", elapsed}});

// AppHost::tick() — safety net
if (frameSeq_ != drawnSeq_) {
    rt.log().warn("AppHost", "missed frame, requesting redraw",
        {{"app", app_.name()}, {"frameSeq", frameSeq_}, {"drawnSeq", drawnSeq_}});
    rt_.view().requestRedraw();
}

// AppHost::draw() entry
rt.log().trace("AppHost", "draw",
    {{"app", app_.name()}, {"n", drawCount}, {"hasFrame", hasFrame}});

// LcdDriver flush
rt.log().trace("LcdDriver", "spi flush",
    {{"bytes", len}, {"elapsedUs", elapsed}});
```

### Fase 2 — Reproduksi (1 hari)

1. Flash firmware fresh ke SkyRizz E32
2. Buka serial monitor, capture semua log
3. Buka Counter app dari launcher. Catat: render atau blank?
4. Tutup Counter, buka SysInfo. Catat yang sama
5. Ulangi 20x (power cycle atau app switch)
6. Pola: apakah blank lebih sering setelah cold boot? Atau setelah app switch?

### Fase 3 — Fix kandidat (1–2 hari)

Berdasarkan hasil Fase 2, pilih satu atau lebih:

**Jika A (race draw-present):**
- Di `AppHost::present()`, selain `requestRedraw()`, juga set `drawnSeq_ = -1`
  supaya safety net di `tick()` langsung trigger
- Atau: `requestRedraw()` sync (post event + yield sampai GuiService proses)

**Jika B (DPM early sleep):**
- Reset DPM timer setiap ada app launch
- Atau: delay DPM start 10 detik setelah boot

**Jika C (SPI/flush corruption):**
- Tambah `vTaskDelay(1)` setelah SPI transaction
- Verifikasi DMA channel tidak dishare
- Gunakan `SPI_TRANS_USE_RXDATA` jika perlu

**Jika D (QuickJS GC):**
- Kurangi JS heap size atau disable GC selama frame render
- Atau: tambah `JS_RunGC()` explicit setelah app init

**Jika E (backlight):**
- Pastikan `Board::initDisplay()` set backlight ON di power-on sequence
- Safety net: GuiService loop set backlight ON setiap frame jika dalam state awake

## 4. Files

| File | Perubahan |
|------|-----------|
| `firmware/core/src/app/app_host.cpp` | Tambah log trace, fix race draw-present |
| `firmware/core/src/services/gui_service.cpp` | Tambah log trace flush |
| `firmware/boards/skyrizz-e32/src/skyrizz_e32_board.cpp` | Cek backlight init |
| `firmware/boards/skyrizz-e32/src/lcd_driver.cpp` | Tambah log trace SPI flush |
| `firmware/core/src/services/dpm_service.cpp` | Delay DPM setelah app launch |

## 5. Acceptance Criteria

- [ ] Counter app selalu render di first launch (20/20 kali)
- [ ] SysInfo app selalu render di first launch (20/20 kali)
- [ ] App switch Counter→SysInfo→Counter tidak pernah blank
- [ ] Cold boot → buka app langsung → render (tidak perlu wait/dummy interact)
- [ ] Log tracer menunjukkan draw ≥ present sequence selalu benar
- [ ] Tidak ada regresi di simulator build
