# Font System

> How fonts work in Palanu: compiled-in defaults, dynamic font packs, the `.bmf`
> binary format, and source-size selection notes.

---

## Overview

The font system has two layers:

1. **Compiled-in fonts** — Helvetica-derived proportional bitmaps generated from u8g2
   BDF sources, compiled as C arrays, always available with zero I/O cost.
2. **Dynamic font packs** — directories of `.bmf` files on VFS that the user can
   select at runtime via Settings → Appearances → Font.

The active font set is tracked by `FontRegistry` (singleton). All UI code accesses
fonts through semantic handles (`Fonts::Reg8`, `Fonts::Bold10`, etc.) so switching a
pack requires only updating the registry.

---

## FontRegistry

Header: `nema/ui/font_registry.h`

```cpp
auto& reg = nema::display::FontRegistry::instance();

// Register a single font (GuiService boot, or compiled-in restore)
reg.registerFont(Fonts::Reg8, &FONT_REG8, "reg8");

// Load an entire pack from VFS — returns false if directory not found
reg.applyFontPack(rt.fs(), "/system/assets/fonts/IoskeleyMono");

// Enumerate available packs (for Settings UI)
char names[8][48]; char paths[8][96];
int n = reg.scanFontPacks(rt.fs(), "/system/assets/fonts/", names, paths, 8);
```

`applyFontPack()` opens each of `reg8.bmf`, `bold8.bmf`, `reg10.bmf`, `bold10.bmf`,
`reg12.bmf`, `bold12.bmf` from the given directory and re-registers the corresponding
`Fonts::*` handles. Missing files in a pack are silently ignored (the prior font stays
for that handle).

---

## Font handles and roles

| Handle | Default (Helvetica) | Semantic role |
|---|---|---|
| `Fonts::Reg8` | `FONT_REG8` | Body text, list items, value labels |
| `Fonts::Bold8` | `FONT_BOLD8` | Small headers, badges |
| `Fonts::Reg10` | `FONT_REG10` | Medium body |
| `Fonts::Bold10` | `FONT_BOLD10` | Primary / screen titles |
| `Fonts::Reg12` | `FONT_REG12` | Large text |
| `Fonts::Bold12` | `FONT_BOLD12` | BigNum (clock digits, counters) |
| `Fonts::Primary` | `FONT_BOLD10` | Alias for Bold10 |
| `Fonts::Secondary` | `FONT_REG8` | Alias for Reg8 |
| `Fonts::Mono` | `FONT_6X8` | Fixed-width terminal text |
| `Fonts::Tiny` | `FONT_REG8` | Alias for Reg8 (compact contexts) |
| `Fonts::BigNum` | `FONT_BOLD12` | Alias for Bold12 |

---

## `.bmf` binary format

Column-major 1-bit bitmap, one byte per column (two bytes for chars taller than 8px).
Produced by `tools/fonts/ttf_encode.py --bmf --prop`.

```
Offset  Size  Field
0       1     Magic = 0xBF
1       1     Version = 1
2       1     charW (nominal / max advance width in pixels)
3       1     charH (cell height in pixels)
4       1     firstChar (ASCII code of first glyph, always 32 = space)
5       1     numChars (number of glyphs)
6       1     spacing (extra inter-glyph gap, 0 for proportional packs)
7       1     bytesPerCol (1 if charH ≤ 8, 2 if charH ≤ 16)
8       1     hasWidths (1 = proportional, per-glyph width table follows header)
9       1     hasOffsets (1 = per-glyph byte offsets follow widths)
10      2     dataSize (LE uint16, size of the bitmap data section)
---
[numChars bytes]    widths[]  — per-glyph advance width (if hasWidths=1)
[numChars × 2 bytes] offsets[] — uint16_t byte offset into data for each glyph
[dataSize bytes]   bitmap data — column-major, bytesPerCol bytes per column
```

`BmfLoader` (part of `FontRegistry`) parses this format at `applyFontPack()` time and
constructs a `BitmapFont` in heap memory.

---

## Shipped font packs

### IoskeleyMono (`/system/assets/fonts/IoskeleyMono/`)

A proportional monospace pixel font. Six sizes: reg8, bold8, reg10, bold10, reg12,
bold12. Source: [IoskeleyMono](https://github.com/ahatem/IoskeleyMono) by Ahmed Hatem.

| File | Source size | charH | charW (max) | Notes |
|---|---|---|---|---|
| reg8.bmf | 9px TTF | 12 | 5 | Source bumped 8→9 — at true 8px the 'm' advance (5px) merges its three arches into a solid block |
| bold8.bmf | 9px TTF | 12 | 5 | Same reason as reg8 |
| reg10.bmf | 11px TTF | 14 | 6 | — |
| bold10.bmf | 11px TTF | 14 | 7 | — |
| reg12.bmf | 13px TTF | 16 | 8 | — |
| bold12.bmf | 13px TTF | 16 | 8 | — |

**Why source size ≠ display size for reg8/bold8:** FreeType `mode='1'` (true
monochrome) scales the outline to the requested pixel height. At 8px the glyph cell
is 11px tall but the advance width stays at 5px. At that width, 'm' three-arch
structure collapses — all columns are filled. Source 9px gives a 12px tall cell with
the same 5px advance but enough vertical room for the arches to separate visually.
The `IoskeleyMono-Term` variant provides improved TTF hinting but those hints only
affect grayscale/subpixel rendering; they are irrelevant to `mode='1'`.

### IoskeleyMono-Condensed (`/system/assets/fonts/IoskeleyMono-Condensed/`)

Narrower variant of the same family. Same size table as IoskeleyMono. charW max = 4
(reg8/bold8) to 7 (reg12/bold12). Useful for denser status-bar or small-screen layouts.

---

## Converter tool

`tools/fonts/ttf_encode.py` — converts a TTF to `.cpp` (C array for `BitmapFont`) or
`.bmf` binary.

```bash
# Generate a .bmf for one size
python3 ttf_encode.py IoskeleyMono-Regular.ttf 9 out.bmf UNUSED UNUSED 0 --prop --bmf

# Regenerate the full IoskeleyMono pack (run from tools/fonts/)
for size in 9 11 13; do
  python3 ttf_encode.py sources/IoskeleyMono-Regular.ttf $size \
    ../../firmware/targets/skyrizz-e32/data/assets/fonts/IoskeleyMono/reg$((size-1)).bmf \
    UNUSED UNUSED 0 --prop --bmf
  # repeat for Bold, Condensed variants
done
```

After regenerating `.bmf` files for the ESP32 target, regenerate the WASM pack headers:

```bash
python3 tools/fonts/gen_pack_header.py \
    firmware/targets/skyrizz-e32/data/assets/fonts/IoskeleyMono/ \
    IoskMono kIoskMono \
    firmware/core/include/nema/assets/fonts/iosk_mono_pack.h
```

The `iosk_mono_pack.h` / `iosk_cond_pack.h` headers are seeded into the WASM
in-memory VFS at startup (`wasm_platform.cpp`) so the Settings Font UI works
identically in the simulator.

---

## Settings UI

Settings → Appearances → Font cycles through discovered packs:

1. `AppearancesSettingsScreen::onResume()` calls `scanFontPacks()`, which:
   - Always inserts "Helvetica" at index 0 (no VFS file needed).
   - Calls `FontRegistry::scanFontPacks()` to list subdirectories of
     `/system/assets/fonts/` that contain at least one `.bmf` file.
2. Left/Right on the Font row calls `cycleFont()` → `applyFont()`.
3. `applyFont()` either restores the compiled-in Helvetica arrays or calls
   `FontRegistry::applyFontPack()` for the selected VFS path.
4. The choice is persisted to config key `"aether"/"font"` and reapplied on next boot
   in `GuiService::start()`.
