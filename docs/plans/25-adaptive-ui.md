# 25 — Adaptive UI (Resolution-Independent Layout)

> UI Palanu menjadi resolution-independent: tidak ada koordinat atau dimensi yang
> di-hardcode ke 264×176. Layout mengikuti resolusi aktual display, dan pada layar
> yang lebih besar/lebih kecil tampilan tetap proporsional dan fungsional.

- Status: 🚧 Phase 1 ✅ (resolution-aware layout, build host+ESP32) · Phase 2 ✅ (Canvas scale factor + dpi hint) · Phase 3 (multi-size fonts) deferred — pixel-doubling sufficient until real 2× hardware exists
- Milestone: M8 (Hardware Portability)
- Depends on: 14 (UI Runtime), 19.6 (GuiService)

---

## Latar belakang & perbandingan

### Bagaimana OS/framework lain menanganinya

| Sistem | Mekanisme | Catatan |
|--------|-----------|---------|
| **Android** | `dp` (density-independent pixel). 1 dp = 1 px di 160 DPI, 2 px di 320 DPI. Font dalam `sp`. Layout constraint-based. | Standar industri mobile |
| **iOS/macOS** | `pt` (points) vs physical px. Retina = 2×/3× scale. Auto Layout + anchors. | Vector fonts (CoreText) |
| **LVGL** (embedded) | `lv_coord_t` = logical pixel. `LV_DPI` global. Font dikompilasi dalam beberapa ukuran bitmap. Tidak ada hardcode resolusi di app. | Paling relevan untuk Palanu |
| **Flipper Zero (Furi)** | Hardcoded 128×64. Tidak responsif sama sekali. | Apa yang TIDAK kita lakukan |
| **TouchGFX (STM32)** | Design-time resolution, tidak runtime-adaptive. | Kurang relevan |

### Pendekatan Palanu

Mengikuti pola LVGL: **logical coordinate system** dengan **integer scale factor**.
Tidak pakai vector font (terlalu berat untuk ESP32), tidak pakai layout engine kompleks
(tidak perlu untuk embedded). Cukup tiga hal:

1. **Hapus semua hardcoded screen dimensions** dari draw code → semua pakai canvas.
2. **Scale factor integer** di Canvas → 1 logical px = N physical px.
3. **Multi-size bitmap fonts** → font yang tepat dipilih otomatis per scale.

---

## Kondisi kode saat ini (audit hasil)

### Yang sudah benar ✓
- `Canvas::width()` / `Canvas::height()` sudah delegate ke `driver_.width/height()` — sudah dynamic
- `GuiService::renderOnce()` sudah pakai `c.width()` / `c.height()` — tidak perlu diubah
- `ui::modalOriginX/Y()` sudah pakai `c.width()` / `c.height()`
- Sebagian besar screens sudah tidak langsung pakai `SCREEN_W`/`SCREEN_H`

### Yang perlu diperbaiki ✗

**`ui_constants.h` — compile-time constants yang harus jadi fungsi:**
```cpp
// ❌ Sekarang (hardcoded):
constexpr uint16_t SCREEN_W     = 264;
constexpr uint16_t SCREEN_H     = 176;
constexpr uint16_t COLS         = SCREEN_W / CHAR_H;  // BUG: harusnya CHAR_W
constexpr uint16_t FOOTER_Y     = SCREEN_H - CHAR_H - 1;
constexpr uint16_t SEP2_Y       = FOOTER_Y - 2;
constexpr uint16_t CONTENT_H    = SEP2_Y - CONTENT_Y;
constexpr uint16_t CONTENT_ROWS = CONTENT_H / CHAR_H;
```

**Occurrences di source files:**

| File | Baris | Apa |
|------|-------|-----|
| `components.cpp` | 14 | `c.fillRect(0, sepY, SCREEN_W, 1)` |
| `status_bar.cpp` | 23 | `(SCREEN_W > rw + 2) ? SCREEN_W - rw - 2 : 0` |
| `status_bar.cpp` | 26 | `c.fillRect(0, SEP1_Y + 1, SCREEN_W, 1)` |
| `about_screen.cpp` | 55 | `c.fillRect(4, y, ui::SCREEN_W - 8, 1)` |
| `about_screen.cpp` | 58 | `if (y + ui::CHAR_H > ui::SCREEN_H - 4) break` |
| `logs_screen.cpp` | 27 | `c.fillRect(0, ..., ui::SCREEN_W, 1)` |
| `sleep_settings_screen.cpp` | 114 | `c.drawText(4, ui::FOOTER_Y, ...)` |
| `virtual_keyboard.cpp` | ~130 | `KW=25, KH=22, STEP=23` hardcoded pixel sizes |

Total: **8 occurrences** — sangat sedikit, Phase 1 bisa selesai cepat.

---

## Tiga fase implementasi

---

## Phase 1 — Resolution-Aware Layout

> **Goal:** Tidak ada pixel dimension yang di-hardcode ke screen dimensions.
> UI berjalan benar di 264×176 (tidak ada perubahan visual), dan sudah siap
> untuk resolusi lain tanpa recompile.

### 1.1 Refactor `ui_constants.h`

Hapus semua konstanta yang bergantung pada screen size. Ganti dengan
inline functions yang menerima `h` (canvas height) atau `w` (canvas width):

```cpp
// HAPUS dari ui_constants.h:
//   SCREEN_W, SCREEN_H, COLS, FOOTER_Y, SEP2_Y, CONTENT_H, CONTENT_ROWS

// TAMBAH di ui_constants.h:
inline uint16_t footerY    (uint16_t h) { return (uint16_t)(h - CHAR_H - 1); }
inline uint16_t sep2Y      (uint16_t h) { return (uint16_t)(h - CHAR_H - 3); }
inline uint16_t contentH   (uint16_t h) { return (uint16_t)(sep2Y(h) - CONTENT_Y); }
inline uint16_t contentRows(uint16_t h) { return contentH(h) / CHAR_H; }
inline uint16_t cols       (uint16_t w) { return w / CHAR_W; }  // fix bug: was /CHAR_H

// Yang TETAP (font metrics — tidak bergantung resolusi):
// CHAR_W, CHAR_H, STATUS_Y, STATUS_H, SEP1_Y, CONTENT_Y
```

**Aturan setelah Phase 1:**
- `ui::SCREEN_W` dan `ui::SCREEN_H` tidak boleh ada di codebase.
- Setiap `draw(Canvas& c)` pakai `c.width()` / `c.height()`.
- Footer dan sep2 dihitung dari `c.height()` saat draw time.

### 1.2 Fix semua 8 occurrences

**`components.cpp` baris 14:**
```cpp
// ❌ Before:
c.fillRect(0, sepY, SCREEN_W, 1);
// ✅ After:
c.fillRect(0, sepY, c.width(), 1);
```

**`status_bar.cpp` baris 23, 26:**
```cpp
// ❌ Before:
uint16_t rx = (SCREEN_W > rw + 2) ? SCREEN_W - rw - 2 : 0;
c.fillRect(0, SEP1_Y + 1, SCREEN_W, 1);
// ✅ After:
uint16_t rx = (c.width() > rw + 2) ? c.width() - rw - 2 : 0;
c.fillRect(0, SEP1_Y + 1, c.width(), 1);
```

**`about_screen.cpp` baris 55, 58:**
```cpp
// ❌ Before:
c.fillRect(4, y, ui::SCREEN_W - 8, 1);
if (y + ui::CHAR_H > ui::SCREEN_H - 4) break;
// ✅ After:
c.fillRect(4, y, c.width() - 8, 1);
if (y + ui::CHAR_H > c.height() - 4) break;
```

**`logs_screen.cpp` baris 27:**
```cpp
// ❌ Before:
c.fillRect(0, ui::CONTENT_Y + ui::CHAR_H + 1, ui::SCREEN_W, 1);
// ✅ After:
c.fillRect(0, ui::CONTENT_Y + ui::CHAR_H + 1, c.width(), 1);
```

**`sleep_settings_screen.cpp` baris 114:**
```cpp
// ❌ Before:
c.drawText(4, ui::FOOTER_Y, "< > change  Cancel back", true);
// ✅ After:
c.drawText(4, ui::footerY(c.height()), "< > change  Cancel back", true);
```

### 1.3 Virtual keyboard — dynamic key sizing

Saat ini: `KW=25, KH=22, STEP=23, LEFT=3` — semuanya hardcoded pixel.

Ganti dengan kalkulasi dari canvas size:
```cpp
// Setiap call ke draw(Canvas& c):
const int LEFT  = 3;
const int KW    = (c.width() - LEFT * 2) / KCOLS;    // per-key width
const int STEP  = KW;                                  // step = key width (no gap)
const int KH    = KW - 1;                             // key height ≈ key width (square-ish)
const int KY    = TOP + 27;
// Remaining height sanity: 4 rows × KH should fit below KY
```

Ini memastikan pada layar 264px wide: KW = (264-6)/10 = 25 ✓ (identik).
Pada layar 400px wide: KW = (400-6)/10 = 39 → key lebih besar, proporsional.

### 1.4 Verifikasi Phase 1

```bash
# Tidak ada SCREEN_W atau SCREEN_H di draw code:
grep -rn "SCREEN_W\|SCREEN_H" firmware/core/src/
# Expected: 0 results

# Tidak ada literal 264 atau 176 di UI code:
grep -rn "\b264\b\|\b176\b" firmware/core/src/ui/ firmware/core/src/screens/
# Expected: 0 results

# Build clean:
cmake --build build
```

---

## Phase 2 — Scale Factor

> **Goal:** Canvas memahami "logical pixel" vs "physical pixel".
> App code di scale 1× → di layar 2× fisik, setiap logical pixel menjadi 2×2 physical pixels.
> Layar 528×352 dengan scale=2 terasa identik dengan 264×176 scale=1.

### 2.1 Desain logical coordinate system

```
Physical resolution: driver_.width() × driver_.height()
Scale factor:        uint8_t scale_ (1, 2, 3 — integer only)
Logical resolution:  width() = physical_width / scale_
                     height() = physical_height / scale_

App code always works in LOGICAL coordinates.
Canvas draw calls multiply by scale_ before touching driver.
```

**Mengapa integer-only?**
Bitmap fonts tidak bisa di-scale fractional tanpa antialiasing (yang tidak ada di 1-bit mono).
Integer scale = pixel-perfect, zero overhead untuk ESP32.

### 2.2 Perubahan `canvas.h`

```cpp
class Canvas {
public:
    explicit Canvas(IDisplayDriver& driver, uint8_t scale = 1);

    uint16_t width()  const { return driver_.width()  / scale_; }
    uint16_t height() const { return driver_.height() / scale_; }
    uint8_t  scale()  const { return scale_; }

    // All draw methods work in LOGICAL coordinates.
    // Internally: physical_x = logical_x * scale_, etc.
    void drawPixel(uint16_t x, uint16_t y, bool on = true);
    void fillRect (uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on = true);
    // ... rest unchanged in signature

private:
    IDisplayDriver& driver_;
    const BitmapFont* font_ = &FONT_5X8;
    uint8_t scale_ = 1;
};
```

### 2.3 Perubahan `canvas.cpp`

Setiap draw primitive mengalikan ke physical space:
```cpp
void Canvas::drawPixel(uint16_t x, uint16_t y, bool on) {
    // scale_ == 1: identical to before (zero overhead branch)
    if (scale_ == 1) { driver_.drawPixel(x, y, on); return; }
    driver_.fillRect((uint16_t)(x * scale_), (uint16_t)(y * scale_),
                     scale_, scale_, on);
}

void Canvas::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    driver_.fillRect((uint16_t)(x * scale_), (uint16_t)(y * scale_),
                     (uint16_t)(w * scale_), (uint16_t)(h * scale_), on);
}
// drawRect, drawLine, invertRect: same pattern
```

Text drawing menggunakan `drawTextScaled(x, y, text, scale_, on)` internaly:
```cpp
void Canvas::drawText(uint16_t x, uint16_t y, const char* text, bool on) {
    if (scale_ == 1) { /* existing path */ return; }
    drawTextScaled(x, y, text, scale_, on);
}
```

`textWidth()` / `textHeight()` return **logical** size:
```cpp
uint16_t Canvas::textHeight() const { return (uint16_t)(font_->charH * scale_); }
```

### 2.4 Scale auto-detection dari driver

`IDisplayDriver` mendapat optional `dpi()` hint (default 0 = unknown):
```cpp
struct IDisplayDriver {
    // ... existing methods ...
    virtual uint16_t dpi() const { return 0; }  // 0 = unknown/unset
};
```

Canvas atau runtime dapat auto-compute scale berdasarkan DPI:
```cpp
// Reference: 264×176 di ~117 DPI → scale=1
// 528×352 di ~234 DPI → scale=2
// Alternatively: explicit scale config via IConfigStore "display/scale"
uint8_t Canvas::autoScale(uint16_t physW, uint16_t physW_ref = 264) {
    return (uint8_t)(physW / physW_ref);  // floor division
}
```

Alternatif lebih simple: user set `display/scale` di config store (default 1).
Runtime baca saat `registerServices()` dan pass ke Canvas constructor.

### 2.5 App code — tidak ada perubahan

App code di Phase 1 tidak perlu diubah. Semua sudah pakai `c.width()`/`c.height()`
yang return logical dimensions. Scale transparan.

### 2.6 Verifikasi Phase 2

```bash
# Build
cmake --build build

# Smoke test: ubah SimDisplay width/height ke 528×352 sementara,
# verifikasi logical dimensions di Canvas = 264×176 dengan scale=2
# Dan verifikasi rendering identik di web panel
```

---

## Phase 3 — Multi-Size Bitmap Fonts

> **Goal:** Text tetap tajam di scale 2× dan 3×.
> Phase 2 sudah "benar" (pixel-doubled), tapi pixel-doubling menghasilkan
> glyph yang terlihat blocky. Phase 3 memberikan font yang dirancang untuk
> ukuran lebih besar.

### 3.1 Pendekatan font

Dua opsi:

**Opsi A — Pixel-doubling (sudah ada, Phase 2 cukup)**
- `FONT_5X8` di-scale 2× → efektif `10×16`
- Hasil: pixel-doubled, terlihat "blocky" tapi fungsional
- Cost: 0 (sudah ada `drawTextScaled`)

**Opsi B — Font terpisah per ukuran (hasil lebih bagus)**
- `FONT_5X8`: scale 1× (264×176) — existing
- `FONT_10X16`: scale 2× (528×352) — dirancang baru atau generated
- `FONT_15X24`: scale 3× — dirancang baru
- Canvas auto-select berdasarkan `scale_`

**Rekomendasi: Opsi A dulu (gratis), Opsi B nanti saat ada hardware 2× yang nyata.**
Pixel-doubling cukup untuk embedded monochrome — ini yang dilakukan sebagian besar
e-ink firmware. Furi/Flipper juga hanya punya satu font size.

### 3.2 Auto-select font (untuk Opsi B)

Jika Opsi B diimplementasikan:
```cpp
// Tambah di canvas.h:
extern const BitmapFont FONT_10X16;  // in font_10x16.cpp
extern const BitmapFont FONT_15X24;  // in font_15x24.cpp

// Canvas constructor / scale setter:
void Canvas::applyScale(uint8_t s) {
    scale_ = s;
    switch (s) {
        case 2:  font_ = &FONT_10X16; break;
        case 3:  font_ = &FONT_15X24; break;
        default: font_ = &FONT_5X8;   break;
    }
}
```

### 3.3 Font generation tool

Generate `font_10x16.cpp` dari `font_5x8.cpp` dengan pixel-doubling script,
atau desain ulang manual untuk keterbacaan lebih baik.

**Tool minimal (Python):**
```python
# Pixel-double setiap glyph dari FONT_5X8 → FONT_10X16
# Setiap kolom bit → 2 kolom, setiap bit → 2 bits
```

### 3.4 Verifikasi Phase 3

```bash
# Build dengan FONT_10X16
cmake --build build

# Verifikasi: pada Canvas scale=2, font_ == &FONT_10X16
# Visual check: text tidak blocky di layar virtual 528×352
```

---

## File yang berubah per fase

### Phase 1 (8 file, ~20 baris)

| File | Perubahan |
|------|-----------|
| `core/include/palanu/ui/ui_constants.h` | Hapus SCREEN_W/H/COLS/FOOTER_Y/SEP2_Y/CONTENT_H/ROWS; tambah inline functions |
| `core/src/ui/components.cpp` | 1 baris: SCREEN_W → c.width() |
| `core/src/ui/status_bar.cpp` | 2 baris: SCREEN_W → c.width() |
| `core/src/screens/about_screen.cpp` | 2 baris: SCREEN_W/H → c.width()/height() |
| `core/src/screens/logs_screen.cpp` | 1 baris: SCREEN_W → c.width() |
| `core/src/screens/sleep_settings_screen.cpp` | 1 baris: FOOTER_Y → footerY(c.height()) |
| `core/src/ui/virtual_keyboard.cpp` | KW/KH/STEP computed dari c.width() |

### Phase 2 (2 file)

| File | Perubahan |
|------|-----------|
| `core/include/palanu/ui/canvas.h` | Tambah scale_, ubah width()/height() |
| `core/src/ui/canvas.cpp` | Semua draw primitives scale by scale_ |
| `core/include/palanu/hal/display.h` | Tambah `virtual uint16_t dpi() const { return 0; }` |

### Phase 3 (3 file)

| File | Perubahan |
|------|-----------|
| `core/src/ui/font_10x16.cpp` | File baru — FONT_10X16 data |
| `core/src/ui/font_15x24.cpp` | File baru — FONT_15X24 data (opsional) |
| `core/include/palanu/ui/canvas.h` | `applyScale()` auto-select font |
| `core/CMakeLists.txt` | Tambah font sources |

---

## Acceptance criteria

### Phase 1
- [ ] `grep -r "SCREEN_W\|SCREEN_H" firmware/core/src/` → 0 hasil
- [ ] Tidak ada literal `264` atau `176` di `src/ui/` dan `src/screens/`
- [ ] Build clean simulator + ESP32
- [ ] Pada 264×176: rendering identik dengan sebelum Phase 1 (visual regression check)
- [ ] Virtual keyboard muncul dan berfungsi normal

### Phase 2
- [ ] `Canvas(driver, 2)` pada 528×352 driver → `canvas.width() == 264`, `canvas.height() == 176`
- [ ] Text dan shapes render di posisi yang benar secara logical
- [ ] Scale=1 (default): zero performance overhead (tidak ada multiply extra)
- [ ] Build clean

### Phase 3
- [ ] `FONT_10X16` terdefinisi dan di-load di Canvas scale=2
- [ ] Visual: text di scale=2 tidak lebih blocky dari pixel-doubling
- [ ] Auto-select font terjadi di constructor/applyScale()

---

## Non-Goals (eksplisit)

- Vector fonts / TTF rendering — terlalu berat untuk ESP32
- Fractional scaling (1.5×, 0.75×) — tidak perlu untuk bitmap font
- Layout engine (flexbox, constraints, anchors) — overkill untuk embedded UI ini
- Font hinting / antialiasing — tidak ada di 1-bit monochrome
- Per-widget DPI — scale factor adalah global untuk satu display
- Landscape/portrait rotation — plan terpisah jika dibutuhkan
