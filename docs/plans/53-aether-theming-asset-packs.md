# 53 — Aether Theming & Asset Packs

> Theme/Style context + namespace asset logis + pack loader (font/icon/animasi) +
> settings (ganti font/scale/style/pack) — **milik Aether**, gaya Momentum Flipper.
> (Server masa depan punya theming sendiri.)

- Status: 🟧 Detail draft (belum diimplementasi)
- Depends on: 50 (UI SDK model), 52 (rasterisasi Aether)

---

## Goals

- **Theme context Aether** (fontSet, scale, palette/style tokens, active pack)
  sebagai state milik Aether; widget & toolkit Aether membacanya.
- **Asset pack**: handle logis (`icon("wifi")`, `font("primary")`) → resolve oleh
  pack aktif; loader pack (fonts/icons/anims); persistence.
- UI Settings: ganti font/scale/style/pack → semua app Aether re-skin.

## Keputusan

- App Aether mereferensi **handle logis**, bukan bitmap mentah → reskin global mungkin.
- Theme/asset = **state milik Aether** (per-server), bukan global lintas-server.
- "Ganti scale" memanfaatkan resolution-independence (reflow otomatis).

---

## Latar belakang

Aether saat ini **belum punya theming runtime** — yang ada cuma dua benih, dan
keduanya statis/global proses, bukan "state milik server" yang bisa di-reskin:

### Kondisi sekarang (di `firmware/core/`)

| Aspek | File | Kondisi sekarang | Nasib |
|---|---|---|---|
| **Font role → spec** | `include/nema/ui/text_style.h:18,23,28`, `src/ui/text_style.cpp` (~60 LOC) | `FontSpec{font, scale}`, `TextRole{Body,Title,Caption}`→font; `setTextSize(Normal/Large)` **global proses** (pixel-double `FONT_5X8`) | **Diangkat jadi `FontSet` di dalam `Theme`** (per-server, bukan global) |
| **Font tunggal** | `include/nema/ui/canvas.h:8`, `src/ui/font_5x8.cpp` | Hanya `FONT_5X8` (5×8 kolom-packed, ASCII 0x20–0x7E) | **Dipertahankan** sbg font default pack built-in; slot untuk font lain dibuka |
| **Skala** | `include/nema/ui/canvas.h:26` `Canvas(driver, scale)` | `scale` integer (Plan 25) — resolution-independence sudah jalan | **Dipakai ulang** sbg `Theme.scale`; ganti scale = re-init Canvas + reflow |
| **Ikon** | — | **Tidak ada** handle ikon; cuma `Canvas::drawBitmap` mentah | **Baru**: `IconHandle` + tabel resolve pack |
| **Animasi** | `src/ui/virtual_keyboard.cpp` dst. ad-hoc | Tidak ada sistem anim asset | **Baru**: `AnimHandle` multi-frame (gaya Flipper IconAnimation) |
| **Persistence** | konfig `display/text_size` (string) | Ada infra config key/value | **Dipakai ulang**: `aether/pack`, `aether/font`, `aether/scale`, `aether/style` |
| **Style tokens** | tersebar di `widgets.cpp`/`renderer.cpp` | padding/gap/border **hardcoded** di builder & paint | **Disentralkan** jadi `StyleTokens` dalam `Theme` |

Jadi pekerjaan inti = **mengangkat konstanta tersebar + preferensi global-proses
menjadi satu `Theme` object yang dimiliki Aether**, lalu menambah lapisan
**AssetPack** (resolve handle logis) + **loader dari storage** + **UI Settings**.

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (LVGL) | Keputusan Aether (Palanu) |
|---|---|---|---|
| **Font handle** | `Font` enum (`FontPrimary/Secondary/Keyboard/BigNumbers/BatteryPercent`) — canvas minta handle, bukan bitmap | `lv_font_t` statis (`fonts.c`), compile-time, tak bisa swap runtime | `FontRole`→handle logis di `FontSet`; pack aktif resolve. **Runtime swap** (Akira tak bisa) |
| **Asset pack** | `lib/momentum/asset_packs.{c,h}`: `AssetPacks{ IconSwapList, fonts[FontTotalNumber], font_params[] }`. Pack di SD: `asset_packs/<Pack>/Icons/...`, `/Fonts/<name>.u8f` | Tak ada konsep asset pack | **Tiru langsung**: pack = dir di storage; built-in pack di flash; user pack override |
| **Reskin ikon** | `asset_packs_swap_icon(requested)` → kembalikan ikon pengganti bila pack override; canvas transparan | — | `pack->icon(handle)` resolve override→fallback built-in. Sama persis |
| **Font file** | `.u8f` (u8g2) di-load ke RAM saat init pack | font compile-time | Format font Aether (lihat di bawah) di-load loader; default tetap `FONT_5X8` |
| **Animasi** | `IconAnimation` (frame_count, frame_rate, frames[]); meta-file `.meta` (w,h,frame_count,fps) | LVGL anim engine (berat) | `AnimAsset{frames[], fps}` ringan, gaya Flipper meta |
| **Pemilihan pack** | Momentum Settings → "Interface › Graphics" pilih pack; persist di `momentum_settings`; `asset_packs_init()` saat boot | Kconfig (compile-time) | UI Settings Aether → tulis config → `Theme` reload → reskin global. Runtime, bukan reboot |
| **Cakupan tema** | Per-firmware (global) | Per-build | **Per-server (Aether)** — server lain (Aurora) punya theming sendiri (Plan 50) |

> Momentum = cetak biru paling pas: handle logis + pack di storage + swap runtime +
> persist. Perbedaan Aether: tema = **state milik server** (bukan global firmware),
> dan **scale** memanfaatkan resolution-independence Palanu (Plan 25) — Flipper
> resolusi tetap 128×64, Palanu reflow.

---

## Desain teknis

### Struktur `Theme` (state milik Aether)

```cpp
namespace aether::ui {

enum class FontRole : uint8_t {           // handle logis (gaya Flipper Font enum)
    Primary, Secondary, Caption, BigNumber, Keyboard
};

struct FontSet {                          // role → font konkret (di-resolve pack)
    const nema::BitmapFont* roles[5];     // diisi dari pack aktif
    uint8_t                 scaleFor[5];  // pixel-double sementara (Plan 25)
};

struct Palette {                          // 1-bit sekarang; slot warna utk server warna
    bool invert      = false;             // dark/light: tukar fg/bg
    // (server warna masa depan: fg/bg/accent RGB di sini)
};

struct StyleTokens {                      // konstanta yg KINI hardcoded di widgets/renderer
    uint8_t  padding   = 2;
    uint8_t  gap       = 2;
    uint8_t  radius    = 2;               // sudut rounded frame/box
    uint8_t  separator = 1;
    enum FocusStyle { Invert, Ring } focus = Invert;
};

struct Theme {                            // DIMILIKI Aether (bukan global lintas-server)
    FontSet           fonts;
    uint8_t           scale  = 1;         // 1..3 — resolution-independence (Plan 25)
    Palette           palette;
    StyleTokens       tokens;
    const AssetPack*  pack;               // pack aktif (resolve icon/font/anim)
};

} // namespace aether::ui
```

`Theme` dibaca oleh **tier-1 toolkit** (`aether::ui::draw`, Plan 52) dan
**renderer** (`paint()`): `padding`/`radius`/`focus`/`separator` tak lagi
hardcoded, font lewat `fonts.roles[role]`, ikon lewat `pack->icon(...)`.

### Struktur `AssetPack` (handle logis → resolve)

```cpp
namespace aether::ui {

using IconHandle = const char*;           // "wifi", "battery", "warning", ...
using AnimHandle = const char*;           // "spinner", "boot", ...

struct Icon { uint16_t w, h; const uint8_t* xbm; };      // 1-bit XBM (drawBitmap)
struct AnimAsset { const Icon* frames; uint8_t count; uint8_t fps; };

struct AssetPack {
    const char* name;                     // "Default", "Momentum", "WatchDogs"

    // Resolve handle logis: override pack → fallback ke built-in flash.
    const nema::BitmapFont* font(FontRole r)   const;
    const Icon*             icon(IconHandle h) const;     // ala asset_packs_swap_icon
    const AnimAsset*        anim(AnimHandle h) const;

private:
    // Tabel built-in (di flash) + overrides (di-load dari storage oleh loader).
    // Lookup by-name; miss → kembalikan built-in default (jangan pernah null).
};

} // namespace aether::ui
```

**Layout pack di storage** (tiru Momentum, di LittleFS/SD):

```
aether_packs/<PackName>/
  manifest.json          # name, font roles, daftar icon/anim
  fonts/<role>.afont     # font Aether (atau .u8f bila adopsi u8g2 nanti)
  icons/<name>.xbm       # 1-bit XBM
  anims/<name>/          # frame*.xbm + meta (count, fps)
```

> **Built-in pack "Default"** = di flash (font `FONT_5X8` + ikon sistem inti:
> wifi/battery/warning/charging). Selalu ada → fallback aman saat storage kosong.
> Pack user di storage hanya **override** handle yang ia sediakan.

### ThemeContext + reskin global

```cpp
namespace aether::ui {

class ThemeContext {                      // service Aether — pemegang Theme aktif
public:
    const Theme& theme() const;

    void setPack(const char* name);       // load pack → swap → invalidate semua surface
    void setFontSet(/* role overrides */);
    void setScale(uint8_t s);             // re-init Canvas (Plan 25) + reflow
    void setStyle(const StyleTokens&);
    void setInvert(bool);

    void load();                          // dari config saat boot (persist)
    void save();                          // tulis config

private:
    Theme        theme_;
    AssetPack    builtin_;                // pack flash, selalu tersedia
    // Saat setX → tandai compositor (Plan 52) dirty → semua surface re-build+repaint.
};

} // namespace aether::ui
```

- **Persistence** (pakai infra config yang ada, `display/text_size` dst):
  `aether/pack` (nama), `aether/font` (override role), `aether/scale` (1..3),
  `aether/style` (token preset), `aether/invert` (bool).
- **Reskin global**: app mereferensi handle logis, jadi `setX()` → ThemeContext
  reload → **compositor invalidate** → tiap surface build ulang node-tree +
  repaint dengan font/icon/token baru. Tanpa app tahu apa-apa.
- **Ganti scale**: `setScale()` re-inisialisasi `Canvas(driver, scale)` (Plan 25);
  layout resolution-independent reflow otomatis (gambar dari `canvas.width/height`).

### UI Settings (gaya Momentum "Interface › Graphics")

Layar Settings Aether (pakai modul `List`/`Select` dari Plan 52):

```
Appearance
  Asset Pack    < Momentum >      → setPack()
  Font          < Primary  >      → setFontSet()
  Text Size     < Large    >      → setScale()  (Normal/Large/XL = scale 1/2/3)
  Style         < Rounded  >      → setStyle()
  Dark Mode     [ ON ]            → setInvert()
```

Tiap perubahan langsung memanggil `ThemeContext::setX()` + `save()` → seluruh app
Aether yang berjalan re-skin tanpa restart (live preview).

### Contoh penggunaan handle (app code)

```cpp
// App TIDAK pernah pegang bitmap; selalu handle logis → reskin global gratis.
draw::icon(c, x, y, "wifi");                 // resolve pack aktif
draw::text(c, x, y, "Battery", TextRole::Caption /*→FontRole via theme*/);
Header(a, "Settings");                        // Header pakai FontRole::Primary dari Theme
```

---

## Fase

- [ ] **Fase 1 — `Theme` + sentralisasi token.** Angkat `text_style.*` jadi
      `FontSet`; pindahkan padding/gap/radius/focus dari `widgets.cpp`/`renderer.cpp`
      ke `StyleTokens`. Renderer + tier-1 toolkit (Plan 52) baca `Theme`. Default
      statis = parity visual sekarang. Host snapshot.
- [ ] **Fase 2 — `AssetPack` + handle resolve.** `IconHandle`/`FontRole`/`AnimHandle`
      + built-in pack "Default" di flash (FONT_5X8 + ikon sistem). `pack->icon/font/anim`
      override→fallback. Ganti `Canvas::drawBitmap` mentah di kode sistem jadi
      `draw::icon(handle)`.
- [ ] **Fase 3 — Loader pack dari storage + persistence.** Parse `manifest.json`,
      load fonts/icons/anims dari `aether_packs/<name>/`. `ThemeContext::load/save`
      via config (`aether/*`). Boot: muat pack tersimpan, fallback "Default".
- [ ] **Fase 4 — Reskin global (ThemeContext ↔ compositor).** `setX()` → invalidate
      semua surface (Plan 52) → rebuild+repaint. Verifikasi ganti pack/font/scale/
      style/invert mengubah semua app berjalan tanpa restart.
- [ ] **Fase 5 — UI Settings "Appearance".** Layar Settings (modul List/Select Plan 52)
      → panggil `setPack/setFontSet/setScale/setStyle/setInvert` + live preview.

**Build/uji:** host + WASM tiap fase; ESP32 build-only Fase 2 & 3 (font/icon di flash
+ baca storage).

---

## File yang disentuh

**Dipertahankan + diangkat:**
- `firmware/core/include/nema/ui/text_style.h` · `src/ui/text_style.cpp` — jadi
  `FontSet`/`FontRole` dalam `Theme` (tak lagi global proses).
- `firmware/core/src/ui/widgets.cpp` · `src/ui/renderer.cpp` — baca `StyleTokens`/
  `FontSet` dari `Theme`, bukan konstanta hardcoded.
- `firmware/core/src/ui/font_5x8.cpp` — jadi font default pack "Default".
- `firmware/core/include/nema/ui/canvas.h` (`scale`) — dipakai `Theme.scale` (Plan 25).

**Baru:**
- `firmware/core/include/nema/ui/theme.h` · `src/ui/theme.cpp` — `Theme`, `FontSet`,
  `Palette`, `StyleTokens`.
- `firmware/core/include/nema/ui/asset_pack.h` · `src/ui/asset_pack.cpp` — `AssetPack`,
  `Icon`, `AnimAsset`, tabel built-in + resolve.
- `firmware/core/include/nema/ui/theme_context.h` · `src/ui/theme_context.cpp` —
  `ThemeContext` (service Aether) + persistence config.
- `firmware/core/src/ui/asset_loader.cpp` — parse manifest + load font/icon/anim
  dari storage.
- Aset built-in: `firmware/core/assets/icons/*.xbm` (atau header ter-generate).
- App Settings "Appearance" (target/app Aether) — pakai modul Plan 52.

**Integrasi:**
- `firmware/core/src/ui/draw_toolkit.cpp` (Plan 52) — `draw::icon`/`draw::text` baca pack.
- `firmware/core/src/ui/compositor.cpp` (Plan 52) — hook invalidate saat tema berubah.

---

## Test

- **Host — resolve handle:** `pack->icon("wifi")` kembalikan override saat ada,
  fallback built-in saat tidak, **tak pernah null** (handle tak dikenal → default).
- **Host — Theme tokens:** render tree dengan dua `StyleTokens` berbeda
  (padding/radius/focus) → snapshot beda; dengan token default → parity sekarang.
- **Host — font role:** `FontSet` berbeda mengubah `measureTextW`/layout secara
  konsisten (layout & paint sepakat — invarian `text_style`).
- **Host — loader:** parse `manifest.json` contoh, muat font/icon/anim dari dir
  fixture; manifest rusak → fallback "Default" + `rt.log().warn`.
- **Host — reskin:** ubah pack/scale/invert via `ThemeContext` → compositor tandai
  semua surface dirty → node tree dibangun ulang dengan aset baru.
- **Persistence:** `save()` lalu `load()` memulihkan pack/font/scale/style/invert
  dari config.
- **WASM:** ganti pack di settings → app JS berjalan re-skin (handle logis).
- **ESP32 build-only:** aset built-in muat di flash; baca pack dari LittleFS/SD;
  cek RAM saat load font/icon ke buffer.

---

## Risiko & mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Load font/icon dari storage makan RAM besar di MCU | OOM | Built-in pack di flash (XIP, nol RAM); user pack load lazy + batasi ukuran; lepas saat ganti pack |
| Handle logis tak dikenal → null deref di renderer | Crash UI | `AssetPack` **selalu** fallback ke built-in (kontrak no-null); test menjamin |
| Reskin global = rebuild semua surface tiap ganti → lag/flicker | UX buruk | Reskin = event jarang (settings); rebuild sekali + repaint penuh; bukan per-frame |
| Ganti scale re-init Canvas saat app jalan | State hilang/artefak | Scale lewat re-init terkontrol di ThemeContext; layout reflow (Plan 25) tak butuh app tahu |
| Format font/icon Aether vs `.u8f` Momentum | Tak kompatibel pack Flipper | v1 pakai XBM 1-bit + font Aether sederhana; adopsi `.u8f`/u8g2 = opsi nanti, slot disiapkan |
| Tema bocor jadi global lintas-server (melanggar keputusan) | Arsitektur rusak | `Theme`/`ThemeContext` hidup di namespace `aether::ui`, dimiliki AetherServer — server lain instansiasi sendiri |
| Manifest pack korup / versi beda | Pack gagal muat senyap | Validasi manifest + versi; gagal → fallback "Default" + `rt.log().error` jelas |
