# 71 — Flipper-Compatible Asset Loader & Build Pipeline

> **Hybrid approach**: runtime `.bm` loader (bitmap/animation) + build-time font pipeline.
> Prioritas: **performa > kemudahan developer > Flipper compat**.
>
> Plan ini mensupport Plan 70 Phase 5 (Animation Library) dan Phase 6 (Font Manager).

---

## DAFTAR ISI

1. [Arsitektur Overview](#1-arsitektur-overview)
2. [BitmapAsset — Loader .bm Individu](#2-bitmapasset--loader-bm-individu)
3. [AnimAsset — Loader Multi-Frame](#3-animasset--loader-multi-frame)
4. [AssetArena — Memory Strategy](#4-assetarena--memory-strategy)
5. [AssetPackLoader — Loader Full Pack](#5-assetpackloader--loader-full-pack)
6. [Font Build Pipeline](#6-font-build-pipeline)
7. [Integrasi dengan System yang Ada](#7-integrasi-dengan-system-yang-ada)
8. [Phase Eksekusi](#8-phase-eksekusi)
9. [Performa & Optimasi](#9-performa--optimasi)
10. [File Baru / Diubah](#10-file-baru--diubah)
11. [Risks & Mitigasi](#11-risks--mitigasi)

---

## 1. Arsitektur Overview

```
┌─ Asset Sources ─────────────────────────────────────────┐
│  Compile-time (flash)           Runtime (VFS/SD)         │
│  ┌──────────┐                  ┌──────────────────┐     │
│  │ icon_pack│                  │ AssetPackLoader   │     │
│  │ anims    │                  │  ├─ BitmapAsset   │     │
│  │ fonts    │                  │  ├─ AnimAsset     │     │
│  └──────────┘                  │  └─ AssetArena    │     │
│       │                        └────────┬─────────┘     │
│       │                                 │               │
│       └──────────┬──────────────────────┘               │
│                  ▼                                      │
│     ┌────────────────────────┐                          │
│     │  Canvas / Renderer     │                          │
│     │  drawBitmap(ptr,w,h)   │  ← agnostic terhadap     │
│     │  drawChar(ch, font)    │     sumber data          │
│     └────────────────────────┘                          │
└─────────────────────────────────────────────────────────┘
```

**Prinsip utama**: Canvas udah pointer-agnostic (`drawBitmap` terima `const uint8_t*`).
Tinggal tambah layer yang supply pointer dari VFS, bukan cuma dari flash. Tanpa ubah
satu line pun di Canvas atau Renderer.

---

## 2. BitmapAsset — Loader .bm Individu

Flipper `.bm` = **raw 1-bit pixel data, tanpa header**. Dimensi dari filename
(`icon_8x8.bm`) atau `meta.txt` (animasi). Persis kompatibel dengan data yang
diharapkan `Canvas::drawBitmap()`.

### API

```cpp
// asset_loader.h — Plan 71
namespace nema::asset {

struct BitmapAsset {
    std::vector<uint8_t> data;   // owned pixel buffer (width * height / 8)
    uint16_t width;
    uint16_t height;

    // Load from VFS. dimensions from filename convention ("name_WxH.bm")
    // or explicit params. Returns false if file not found / read error.
    bool load(IFileSystem& fs, const char* path);
    bool load(IFileSystem& fs, const char* path, uint16_t w, uint16_t h);

    const uint8_t* bits() const { return data.data(); }
    bool valid() const { return !data.empty(); }

    // Release buffer back to arena (if arena-backed) or free heap
    void release();
};

// Direct-from-VFS draw — reads file each call (for infrequent icons).
// Uses internal small stack buffer (no arena dependency).
bool drawFile(Canvas& c, IFileSystem& fs, const char* path,
              uint16_t x, uint16_t y, uint16_t w, uint16_t h);

} // namespace nema::asset
```

### Alokasi memory

`vector<uint8_t>` dialokasikan di **AssetArena** (Section 4), bukan general heap —
cegah fragmentasi. Arena menggunakan bump allocator: semua alloc contiguous,
reset O(1).

### Filename dimension convention

Jika dimensi tidak di-pass eksplisit, loader auto-detect dari filename:
- `icon_8x8.bm` → w=8, h=8
- `dolphin_20x20.bm` → w=20, h=20
- Fallback: `_WxH.bm` suffix; kalau tidak ada, return error.

---

## 3. AnimAsset — Loader Multi-Frame

Flipper menyimpan animasi sebagai direktori berisi frame `.bm` + `meta.txt`:

```
/dolphin/idle/
  ├─ meta.txt          ← Width, Height, Frame rate, Frames count
  ├─ frame_0.bm
  ├─ frame_1.bm
  └─ frame_29.bm
```

### API

```cpp
namespace nema::asset {

struct AnimAsset {
    Animation         def;          // compatible dengan Animation struct kita
    std::vector<AnimationFrame> frames;  // owns frame metadata
    std::vector<std::vector<uint8_t>> buffers; // owns pixel data per frame

    // Load dari direktori VFS. Baca meta.txt untuk dimensi + timing,
    // lalu load semua frame_N.bm secara sequential (0..N-1).
    bool load(IFileSystem& fs, const char* dirPath);

    // Konversi ke Animation& untuk dipakai AnimationPlayer.
    // AnimationPlayer hanya menyimpan reference — AnimAsset HARUS hidup lebih lama.
    const Animation& animation() const { return def; }

    void release();  // free semua buffer ke arena
};

// Meta.txt parser (subset dari Flipper format)
struct AnimMeta {
    uint16_t width;
    uint16_t height;
    uint8_t  frameCount;   // passive frames (v1: hanya support passive)
    uint8_t  frameRate;    // frames per second
    bool     loop;         // derived: active_cycles > 0 atau Duration > 0
};

AnimMeta parseAnimMeta(const char* txt);

} // namespace nema::asset
```

### Format meta.txt Flipper

```
Filetype: Flipper Animation
Version: 1

Width: 20
Height: 20
Passive frames: 30
Active frames: 10
Active cycles: 2
Frame rate: 2
Duration: 60
Active cooldown: 15
Bubble slots: 0
```

Field yang dipakai: `Width`, `Height`, `Passive frames`, `Frame rate`.
`Active frames` dan `Active cycles` **deferred** — v1 hanya support passive phase.

### Integrasi dengan AnimationPlayer

`AnimAsset::animation()` return `const Animation&` — **langsung kompatibel**
dengan `AnimationPlayer(animAsset.animation())`. Gak perlu ubah `AnimationPlayer`
atau `AnimationManager` sama sekali.

```cpp
// Contoh penggunaan:
AnimAsset dolphin;
dolphin.load(rt.fs(), "/packs/default/animations/dolphin_idle");
AnimationPlayer player(dolphin.animation());
player.start();
AnimationManager::instance().registerPlayer(player);
```

**Lifecycle ownership**: `AnimAsset` HARUS hidup lebih lama dari `AnimationPlayer`.
Screens simpan `AnimAsset` sebagai member, `AnimationPlayer` sebagai member terpisah.

### Ukuran per animasi

Contoh dolphin idle 20×20, 30 frame:
- Pixel data: 30 × (20×20/8) = 30 × 50 = **1,500 bytes**
- Frame metadata: 30 × sizeof(AnimationFrame) = 30 × 8 = 240 bytes
- Vector overhead: ~200 bytes
- **Total: ~2 KB** — negligible bahkan di SRAM

---

## 4. AssetArena — Memory Strategy

Hindari fragmentasi heap dengan arena khusus asset. Bump allocator: semua
alokasi contiguous, tidak ada free() individual — reset arena = O(1) pointer reset.

### Target hardware

| Board | PSRAM | Arena budget | % PSRAM |
|-------|-------|-------------|---------|
| SkyRizz E32 (N16R8) | 8 MB | 256 KB | 3.1% |
| Dev Board (N8R8) | 8 MB | 256 KB | 3.1% |
| WASM Simulator | host | 256 KB | — |

### API

```cpp
namespace nema::asset {

class AssetArena {
public:
    static AssetArena& instance();

    // Allocate fixed block from PSRAM at boot. Called once.
    bool init(size_t sizeBytes = 256 * 1024);

    // Bump-allocator: fast, no fragmentation, no individual free().
    // Returns nullptr if arena exhausted.
    uint8_t* alloc(size_t size);

    // Release ALL allocations — called on screen transition.
    void reset();

    size_t used() const;
    size_t capacity() const;

private:
    uint8_t* block_  = nullptr;
    size_t   size_   = 0;
    size_t   offset_ = 0;
};

} // namespace nema::asset
```

### Lifecycle integration

```
Screen::enter()   → arena.reset()              // free previous screen's assets
                   → load assets via VFS         // BitmapAsset::load() / AnimAsset::load()
Screen::draw()    → pakai data dari arena       // Canvas::drawBitmap()
Screen::onExit()  → arena.reset()              // optional explicit cleanup
```

### Kenapa bump allocator, bukan general heap?

| Aspek | General heap | Bump arena |
|-------|-------------|------------|
| Fragmentasi | Bisa tinggi setelah banyak load/unload | Zero — semua contiguous |
| Free | O(log n) atau O(1) per block | O(1) reset seluruhnya |
| Cache | Tersebar | Data berurutan, cache-line friendly |
| Prediktabilitas | Sulit prediksi sisa | `used() / capacity()` akurat |
| Kecepatan alloc | O(log n) | 1 pointer increment |

---

## 5. AssetPackLoader — Loader Full Pack

Wrapper yang membaca struktur direktori Flipper Momentum asset pack.

### Struktur pack

```
/packs/MyPack/
  ├─ meta.txt           ← pack metadata (name, version) — v1: diabaikan
  ├─ icons/
  │   ├─ wifi_8x8.bm
  │   ├─ bt_8x8.bm
  │   └─ battery_8x8.bm
  ├─ animations/
  │   ├─ dolphin_idle/
  │   │   ├─ meta.txt
  │   │   ├─ frame_0.bm
  │   │   └─ frame_29.bm
  │   └─ spinner/
  └─ fonts/
      ├─ primary.u8f      ← handled by build-time pipeline (Section 6)
      └─ mono.u8f
```

### API

```cpp
namespace nema::asset {

class AssetPackLoader {
public:
    AssetPackLoader(IFileSystem& fs, const char* packPath);

    // Load individual icon by relative path within pack
    BitmapAsset loadIcon(const char* name);  // "icons/wifi_8x8.bm"

    // Load animation by directory name within pack
    AnimAsset   loadAnimation(const char* name);  // "animations/dolphin_idle"

    // Enumerate available assets in pack
    std::vector<std::string> listIcons();
    std::vector<std::string> listAnimations();

    bool isValid() const;

private:
    IFileSystem& fs_;
    std::string  basePath_;   // "/packs/MyPack/"
};

} // namespace nema::asset
```

### AssetPackRegistry

Singleton yang menyimpan mapping handle → loaded BitmapAsset, dipakai sebagai
fallback oleh `findIcon()`:

```cpp
class AssetPackRegistry {
public:
    static AssetPackRegistry& instance();

    void registerIcon(const char* handle, const BitmapAsset& asset);
    const BitmapAsset* find(const char* handle) const;
    void clear();  // remove all (on pack unload)

private:
    std::vector<Entry> entries_;
};
```

---

## 6. Font Build Pipeline

Font **TETAP build-time** — `.u8f` (u8g2 binary format) terlalu kompleks buat
runtime loader. Variable-width encoding, unicode lookup tables, multi-section
glyph data. Build-time converter = simpel, hasil baked ke flash.

### Arsitektur pipeline

```
tools/fonts/
  ├─ encode.py              ← BDF → BitmapFont C array converter
  ├─ sources/               ← BDF files from u8g2 (BSD-2 license)
  │   ├─ u8g2_font_helvB08.bdf
  │   ├─ u8g2_font_profont22.bdf
  │   ├─ u8g2_font_profont11.bdf
  │   └─ u8g2_font_4x6_tf.bdf
  └─ output/                ← generated .cpp files (committed to repo)
      ├─ font_primary.cpp
      ├─ font_bignum.cpp
      ├─ font_mono.cpp
      └─ font_tiny.cpp
```

### encode.py

```python
# BDF → BitmapFont converter
# Input:  BDF file (human-readable bitmap font, u8g2 source format)
# Output: C++ file with const BitmapFont + glyph data array
#
# BDF glyphs are column-major packed (like our BitmapFont), so conversion
# is straightforward: extract glyph bounding boxes, pad to uniform width,
# pack columns into bytes (MSB = top pixel).
#
# Usage: python encode.py sources/helvB08.bdf output/font_primary.cpp

import sys
import re

def parse_bdf(path):
    """Parse BDF file, return { codepoint: [column_bytes...] }"""
    # ... implementation per BDF spec
    pass

def encode_glyph(columns, char_w, char_h):
    """Pack column-major glyph into byte array (char_w bytes per glyph)"""
    result = []
    for col in columns:
        byte = 0
        for row, pixel in enumerate(col):
            if pixel: byte |= (1 << (7 - row))  # MSB = top
        result.append(byte)
    return result

def generate_cpp(font_name, glyphs, char_w, char_h, first_char, num_chars):
    """Emit const BitmapFont + const uint8_t[] data"""
    # ... generate .cpp file
    pass

if __name__ == '__main__':
    bdf_path = sys.argv[1]
    out_path = sys.argv[2]
    # ...
```

### Kenapa BDF, bukan langsung dari repo Flipper?

| Alasan | Detail |
|--------|--------|
| **Source format u8g2** | BDF = format source SEBELUM dikompilasi ke `.u8f`. Flipper juga compile BDF → `.u8f` |
| **License** | u8g2 font BDF sources: BSD 2-clause — bebas pakai, gak copyleft |
| **Pilihan font** | Bisa pilih dari 200+ font u8g2, gak terbatas yang dipakai Flipper |
| **Human-readable** | BDF = text, gampang debug/inspect glyph per glyph |
| **Column-major** | BDF packed column-major — sama seperti BitmapFont kita. Konversi hampir 1:1 |

### Font yang disiapkan

| Font | Source BDF (u8g2) | Glyph Size | Target Handle | Use Case |
|------|-------------------|-----------|---------------|----------|
| Primary | `helvB08` (Helvetica bold 8px) | 5×8 | `Fonts::Primary` | Titles, headers |
| Secondary | `FONT_5X8` (Adafruit 5×7 +1) | 5×8 | `Fonts::Secondary` | Body text (sudah ada) |
| Mono | `profont11` (ProFont 11px) | 6×10 | `Fonts::Mono` | Logs, hex dump, code |
| BigNum | `profont22` (ProFont 22px) | 11×16 | `Fonts::BigNum` | Clock face, counter |
| Tiny | `04b_03` (4×6 tiny) | 4×6 | `Fonts::Tiny` | Status bar condensed |

### Perbandingan dengan Flipper

| Flipper Font | Format | Palanu Equivalent |
|-------------|--------|-------------------|
| `FontPrimary` (HelvB08, 8px) | `.u8f` | **Fonts::Primary** (helvB08 → BitmapFont) |
| `FontSecondary` (haxrcorp, 7px) | `.u8f` | **Fonts::Secondary** (FONT_5X8) |
| `FontKeyboard` (profont, 7px) | `.u8f` | **Fonts::Mono** (profont11 → BitmapFont) |
| `FontBigNumbers` (profont22, 15px) | `.u8f` | **Fonts::BigNum** (profont22 → BitmapFont) |
| `FontBatteryPercent` (5x7, 6px) | `.u8f` | **Fonts::Tiny** (04b_03 → BitmapFont) |

---

## 7. Integrasi dengan System yang Ada

### 7a. IconDef Bridge — findIcon() VFS fallback

`findIcon()` saat ini cuma resolve ke flash. Tambah fallback ke `AssetPackRegistry`:

```cpp
// icon_pack.cpp — extended
const IconDef* findIcon(const char* handle) {
    // 1. Cek built-in flash icons (existing — 16 icons)
    for (const IconDef* d = k_icons; d->handle; ++d)
        if (strcmp(d->handle, handle) == 0) return d;

    // 2. Cek loaded asset pack icons (Plan 71)
    if (auto* loaded = AssetPackRegistry::instance().find(handle))
        return loaded;  // BitmapAsset → IconDef bridge

    return nullptr;
}
```

### 7b. Animation Bridge — langsung kompatibel

`A_SPINNER` tetap di flash. Animasi dari asset pack dibuat via `AnimAsset`:

```cpp
// Contoh di home_screen.cpp atau app screen:
class HomeScreen : public ComponentScreen {
    AnimAsset       dolphinAnim_;
    AnimationPlayer dolphinPlayer_{A_SPINNER};  // default: spinner built-in

    void enter() override {
        if (auto* fs = rt.fs()) {
            if (dolphinAnim_.load(*fs, "/packs/default/animations/dolphin_idle")) {
                dolphinPlayer_ = AnimationPlayer(dolphinAnim_.animation());
            }
        }
        dolphinPlayer_.start();
        AnimationManager::instance().registerPlayer(dolphinPlayer_);
    }
};
```

### 7c. Font Registry — register di boot

```cpp
// gui_service.cpp — boot sequence (registerFonts)
void GuiService::registerFonts() {
    auto& reg = FontRegistry::instance();
    reg.registerFont(Fonts::Primary,   &FONT_PRIMARY,   "primary");
    reg.registerFont(Fonts::Secondary, &FONT_5X8,       "secondary");
    reg.registerFont(Fonts::Mono,      &FONT_MONO_6X10, "mono");
    reg.registerFont(Fonts::Tiny,      &FONT_TINY_4X6,  "tiny");
    reg.registerFont(Fonts::BigNum,    &FONT_BIGNUM,    "bignum");
}
```

### 7d. ViewDispatcher — arena lifecycle

```cpp
// view_dispatcher.cpp — extended
void ViewDispatcher::push(IScreen& screen) {
    // Reset arena so new screen gets fresh allocation space
    AssetArena::instance().reset();
    // ... existing push logic
}
```

---

## 8. Phase Eksekusi

### Phase 1 — Fondasi (1-2 hari)

- [ ] **AssetArena** — bump allocator singleton, `init()` di `GuiService::boot()`
- [ ] **BitmapAsset** — load single .bm dari VFS, filename dimension parsing
- [ ] **drawFile()** — convenience direct-from-VFS draw (pakai stack buffer kecil)
- [ ] Unit test: load file, verify pixel bytes, draw ke Canvas → visual diff
- [ ] Arena reset dipanggil di `ViewDispatcher::push()`

### Phase 2 — Animasi (1-2 hari)

- [ ] **AnimMeta parser** — baca `Width`/`Height`/`Passive frames`/`Frame rate` dari meta.txt
- [ ] **AnimAsset** — load direktori animasi (list `frame_0.bm` ... `frame_N.bm`, sort numeric)
- [ ] Integrasi dengan `AnimationPlayer` via `animation()` — zero changes ke AnimationPlayer
- [ ] Test: load dolphin idle dari Flipper repo, play di HomeScreen sebagai pengganti spinner

### Phase 3 — Pack Loader (1 hari)

- [ ] **AssetPackLoader** — baca struktur direktori Flipper pack (`icons/`, `animations/`)
- [ ] **AssetPackRegistry** — singleton mapping handle → loaded `BitmapAsset`
- [ ] `findIcon()` fallback ke VFS via `AssetPackRegistry`
- [ ] Test: load Flipper asset pack, tampilkan icon di AppListScreen

### Phase 4 — Font Pipeline (1-2 hari)

- [ ] **`tools/fonts/encode.py`** — BDF → BitmapFont C array converter
- [ ] Download 4 BDF source dari u8g2 repo
- [ ] Encode 4 font baru: Primary, Mono, BigNum, Tiny
- [ ] Register di FontRegistry saat boot
- [ ] Migrasi screen: LogsScreen → Mono, LockScreen → BigNum, StatusBar → Tiny

### Phase 5 — Polishing (1 hari)

- [ ] Arena reset lifecycle dipastikan jalan di screen enter/exit
- [ ] Error handling: file not found, corrupted .bm (size mismatch), bad meta.txt
- [ ] Logging: `rt.log().info("Asset", "loaded", {{"path", path}, {"bytes", size}})`
- [ ] Perf test: load time, arena usage per screen, frame timing benchmark
- [ ] SkyRizz E32 + Dev Board + WASM build verification

---

## 9. Performa & Optimasi

| Concern | Strategy | Target |
|---------|----------|--------|
| **Load time** | Load di `enter()` — sebelum screen ditampilkan | Icon 8×8: <1ms |
| **RAM fragmentation** | Arena bump allocator — zero frag, O(1) reset | N/A |
| **PSRAM latency** | Sequential frame access, cache-line friendly | <1ms per frame draw |
| **VFS overhead** | Batch read semua frame sekaligus di `load()` | Animasi 30 frame: <5ms |
| **Konkurensi** | Arena + loader single-threaded (GUI thread only) | No locks |
| **Flash vs RAM** | Ikon kecil/static tetap di flash; animasi besar ke arena | Hybrid optimal |

**Target load time** (ESP32-S3, 16MB flash, 8MB PSRAM):
- Icon 8×8 (8 bytes): **<1ms**
- Animasi 20×20, 30 frame (1,500 bytes): **<5ms**
- Full asset pack (50 icons + 3 animasi): **<50ms**

---

## 10. File Baru / Diubah

| File | Status | Deskripsi |
|------|--------|-----------|
| `firmware/core/include/nema/ui/asset_loader.h` | **NEW** | BitmapAsset, AnimAsset, AnimMeta, AssetPackLoader, AssetArena, AssetPackRegistry |
| `firmware/core/src/ui/asset_loader.cpp` | **NEW** | Implementasi semua loader |
| `firmware/core/src/ui/asset_arena.cpp` | **NEW** | AssetArena singleton init + allocator |
| `tools/fonts/encode.py` | **NEW** | BDF → BitmapFont C array converter |
| `tools/fonts/sources/*.bdf` | **NEW** | 4 BDF font source dari u8g2 |
| `firmware/core/src/ui/font_primary.cpp` | **NEW** | FontPrimary data (generated) |
| `firmware/core/src/ui/font_bignum.cpp` | **NEW** | FontBigNum data (generated) |
| `firmware/core/src/ui/font_mono.cpp` | **NEW** | FontMono data (generated) |
| `firmware/core/src/ui/font_tiny.cpp` | **NEW** | FontTiny data (generated) |
| `firmware/core/src/ui/icon_pack.cpp` | **EDIT** | `findIcon()` VFS fallback via AssetPackRegistry |
| `firmware/core/src/services/gui_service.cpp` | **EDIT** | Arena init + font registration di boot |
| `firmware/core/src/ui/view_dispatcher.cpp` | **EDIT** | Arena reset di `push()` |
| `firmware/core/CMakeLists.txt` | **EDIT** | Tambah file baru ke `NEMA_CORE_SRCS` |

---

## 11. Risks & Mitigasi

| Risk | Mitigation |
|------|------------|
| Arena overcommit (load >256KB) | Deteksi di `load()` — return false, log error, fallback ke placeholder icon/animasi |
| meta.txt parsing error | Strict parser: unknown key = warning (skip), missing required key = fail `load()`, return false |
| VFS lambat di SPI flash (latency) | Acceptable — load hanya sekali di `enter()`, bukan per frame. 5ms load time unnoticeable |
| Animasi Flipper pakai Active/Passive dual phase | v1 hanya support Passive frames; Active frames + Active cycles deferred (gak kritis buat demo) |
| Font BDF encode menghasilkan glyph salah | Visual diff test: generate semua glyph, bandingkan dengan screenshot u8g2/Adafruit GFX |
| PSRAM gak available (misal WASM sim) | `AssetArena::init()` fallback ke heap kalau PSRAM malloc gagal |
| Frame `.bm` tidak sequential (skip number) | List direktori, sort numeric by `frame_N`, bukan sequential scan — robust terhadap gap |
| `AnimationPlayer` reference dangling | `AnimAsset` HARUS jadi member class yang hidup lebih lama. Assert di destructor |

---

## Summary

| Area | Approach | Rationale |
|------|----------|-----------|
| **Bitmap (.bm)** | Runtime loader via VFS | Canvas sudah pointer-agnostic — 0 conversion, langsung kompatibel |
| **Animasi multi-frame** | AnimAsset → Animation bridge | `animation()` return `const Animation&` — langsung cocok ke AnimationPlayer |
| **Font (.u8f)** | Build-time BDF → BitmapFont converter | u8g2 format terlalu kompleks buat runtime; build-time = simpel, hasil di flash |
| **Memory** | 256KB bump arena di PSRAM | Zero fragmentation, O(1) reset, 3% PSRAM budget |
| **Flipper compat** | Full struktur direktori + meta.txt support | Community packs langsung jalan, zero conversion needed |

**Total estimasi**: 5-8 hari kerja. Bisa langsung drop Flipper asset pack, verifikasi
pipeline end-to-end, lalu swap ke asset sendiri ketika siap.
