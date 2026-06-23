#!/usr/bin/env python3
"""
BDF → BitmapFont C array converter (Plan 71 / Plan 79 proportional).

Converts BDF font sources to Palanu BitmapFont format. Two output modes:

  monospace (default): every glyph padded to a fixed charW; glyph i at
                       data + i*charW*bytesPerCol. Back-compat with v1 fonts.

  --prop:              proportional — emits a per-glyph WIDTHS[] table and an
                       OFFSETS[] table; each glyph stores only its own columns.
                       Advance width = BDF DWIDTH; left/vertical bearing baked in
                       via BBX xoff/yoff so every glyph sits on a shared baseline.

Glyph storage (both modes): column-major, `bytesPerCol` bytes per pixel column,
bit 0 = topmost row. Tall glyphs (height 9..16) use bytesPerCol=2 (byte 0 = rows
0..7, byte 1 = rows 8..15).

Usage:
  python encode.py sources/helvR08.bdf output/font_reg8.cpp FONT_REG8 reg8 --prop
  python encode.py sources/4x6.bdf      output/font_tiny.cpp FONT_TINY tiny   # monospace
"""

import sys
import os
from collections import OrderedDict

ASCII_FIRST = 0x20
ASCII_LAST = 0x7E


def parse_bdf(path):
    """Return (glyphs, fbb). glyphs[cp] = {w,h,xoff,yoff,dwidth,pixels(rows of bools)}."""
    with open(path, 'r', encoding='latin-1', errors='replace') as f:
        lines = f.readlines()

    glyphs = OrderedDict()
    in_char = in_bitmap = False
    cp = None
    bbx_w = bbx_h = bbx_x = bbx_y = 0
    dwidth = 0
    rows = []
    fbb = None

    for line in lines:
        line = line.strip()
        if line.startswith('FONTBOUNDINGBOX'):
            p = line.split()
            fbb = (int(p[1]), int(p[2]), int(p[3]), int(p[4]))
        elif line.startswith('STARTCHAR'):
            in_char, rows = True, []
        elif not in_char:
            continue
        elif line.startswith('ENCODING'):
            cp = int(line.split()[1])
        elif line.startswith('DWIDTH'):
            dwidth = int(line.split()[1])
        elif line.startswith('BBX'):
            p = line.split()
            bbx_w, bbx_h, bbx_x, bbx_y = int(p[1]), int(p[2]), int(p[3]), int(p[4])
        elif line == 'BITMAP':
            in_bitmap = True
        elif line == 'ENDCHAR':
            in_char = in_bitmap = False
            if cp is not None:
                byte_w = max(1, (bbx_w + 7) // 8)
                pixels = []
                for rh in rows:
                    rp = []
                    for bi in range(byte_w):
                        bs = rh[bi * 2: bi * 2 + 2]
                        val = int(bs, 16) if bs else 0
                        for co in range(8):
                            col = bi * 8 + co
                            if col >= bbx_w:
                                break
                            rp.append(bool((val >> (7 - co)) & 1))
                    pixels.append(rp)
                glyphs[cp] = {'w': bbx_w, 'h': bbx_h, 'xoff': bbx_x,
                              'yoff': bbx_y, 'dwidth': dwidth, 'pixels': pixels}
            rows = []
        elif in_bitmap and line:
            rows.append(line)

    return glyphs, fbb


def ascii_glyphs(glyphs):
    return {cp: g for cp, g in glyphs.items() if ASCII_FIRST <= cp <= ASCII_LAST}


def encode_columns(cell, bytes_per_col):
    """cell = list of `width` columns, each a list of `height` bools (top→bottom).
    Returns flat byte list, bytes_per_col bytes per column (bit 0 = top row)."""
    out = []
    for col in cell:
        for b in range(bytes_per_col):
            v = 0
            for bit in range(8):
                row = b * 8 + bit
                if row < len(col) and col[row]:
                    v |= (1 << bit)
            out.append(v)
    return out


def build_proportional(glyphs):
    """Compute shared baseline cell + per-glyph column bitmaps.
    Returns (glyph_cells dict cp→list[col][bool], cell_h, bytes_per_col)."""
    ag = ascii_glyphs(glyphs)
    # Shared vertical metrics from the ASCII set.
    ascent = max((g['yoff'] + g['h']) for g in ag.values() if g['h'] > 0)
    descent = max((-g['yoff']) for g in ag.values() if g['h'] > 0)
    if descent < 0:
        descent = 0
    cell_h = ascent + descent
    bytes_per_col = (cell_h + 7) // 8

    cells = {}
    for cp in range(ASCII_FIRST, ASCII_LAST + 1):
        g = ag.get(cp)
        advance = max(1, g['dwidth']) if g else 4
        width = advance
        if g and g['w'] > 0:
            width = max(advance, g['xoff'] + g['w'])
        cell = [[False] * cell_h for _ in range(width)]
        if g and g['w'] > 0 and g['h'] > 0:
            col_left = max(0, g['xoff'])
            row_top = ascent - (g['yoff'] + g['h'])   # rows from top to glyph top
            for r in range(g['h']):
                for c in range(g['w']):
                    if r < len(g['pixels']) and c < len(g['pixels'][r]) and g['pixels'][r][c]:
                        cy = row_top + r
                        cx = col_left + c
                        if 0 <= cy < cell_h and 0 <= cx < width:
                            cell[cx][cy] = True
        cells[cp] = cell
    return cells, cell_h, bytes_per_col


import struct

HEADER = '// Generated by tools/fonts/encode.py (Plan 79) — do not edit.\n// Source: {}\n#include "nema/ui/canvas.h"\n\nnamespace nema {{\n\n'


def emit_bmf(out_path, cells, cell_h, bpc, spacing):
    """Write binary .bmf file (same format as ttf_encode.py --bmf output)."""
    widths = []
    offsets = []
    data_bytes = []
    for cp in range(ASCII_FIRST, ASCII_LAST + 1):
        cell = cells[cp]
        offsets.append(len(data_bytes))
        widths.append(len(cell))
        for col in cell:
            for b in range(bpc):
                v = 0
                for bit in range(8):
                    row = b * 8 + bit
                    if row < len(col) and col[row]:
                        v |= (1 << bit)
                data_bytes.append(v)

    num = ASCII_LAST - ASCII_FIRST + 1
    max_w = max(widths)
    data_size = len(data_bytes)

    # 12-byte header
    header = struct.pack('<BBBBBBBBBBH',
        0xBF,          # magic
        0x01,          # version
        max_w,         # charW (max advance)
        cell_h,        # charH
        ASCII_FIRST,   # firstChar
        num,           # numChars
        spacing,       # spacing
        bpc,           # bytesPerCol
        1,             # hasWidths
        1,             # hasOffsets
        data_size,     # dataSize (LE uint16)
    )

    # Derive .bmf path from out_path (strip .cpp → .bmf, or replace extension)
    bmf_path = os.path.splitext(out_path)[0] + '.bmf'
    with open(bmf_path, 'wb') as f:
        f.write(header)
        f.write(bytes(widths))
        for off in offsets:
            f.write(struct.pack('<H', off))
        f.write(bytes(data_bytes))

    total = len(header) + len(widths) + len(offsets) * 2 + data_size
    print(f'  {os.path.basename(bmf_path)}: {cell_h}px cell, {num} glyphs, '
          f'{data_size}B bitmap + tables = {total}B total')


def emit_proportional(out_path, src, font_var, data_name, cells, cell_h, bpc, spacing):
    data_bytes = []
    widths = []
    offsets = []
    for cp in range(ASCII_FIRST, ASCII_LAST + 1):
        cell = cells[cp]
        offsets.append(len(data_bytes))
        widths.append(len(cell))
        data_bytes.extend(encode_columns(cell, bpc))

    num = ASCII_LAST - ASCII_FIRST + 1
    max_w = max(widths)

    def hexrow(vals):
        return ','.join(f'0x{v:02X}' for v in vals)

    with open(out_path, 'w') as f:
        f.write(HEADER.format(os.path.basename(src)))
        # data
        f.write(f'static const uint8_t {data_name}_DATA[] = {{\n')
        for i in range(0, len(data_bytes), 16):
            f.write('    ' + hexrow(data_bytes[i:i + 16]) + ',\n')
        f.write('};\n\n')
        # widths
        f.write(f'static const uint8_t {data_name}_W[] = {{\n')
        for i in range(0, len(widths), 24):
            f.write('    ' + ','.join(str(v) for v in widths[i:i + 24]) + ',\n')
        f.write('};\n\n')
        # offsets
        f.write(f'static const uint16_t {data_name}_OFF[] = {{\n')
        for i in range(0, len(offsets), 16):
            f.write('    ' + ','.join(str(v) for v in offsets[i:i + 16]) + ',\n')
        f.write('};\n\n')
        # struct
        f.write(f'const BitmapFont {font_var} = {{\n')
        f.write(f'    {data_name}_DATA,\n')
        f.write(f'    {max_w},    // charW (max advance, fallback)\n')
        f.write(f'    {cell_h},    // charH\n')
        f.write(f'    0x{ASCII_FIRST:02X}, // firstChar\n')
        f.write(f'    {num},     // numChars\n')
        f.write(f'    {spacing},     // spacing\n')
        f.write(f'    {data_name}_W,    // widths (proportional)\n')
        f.write(f'    {data_name}_OFF,  // offsets\n')
        f.write(f'    {bpc},     // bytesPerCol\n')
        f.write('};\n\n} // namespace nema\n')

    total = len(data_bytes) + len(widths) + len(offsets) * 2
    print(f'Generated {out_path}: proportional {cell_h}px, {num} glyphs, '
          f'{len(data_bytes)}B bitmap + {total - len(data_bytes)}B tables = {total}B')


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    flags = {a for a in sys.argv[1:] if a.startswith('--')}
    if len(args) < 4:
        print('Usage: encode.py <in.bdf> <out.cpp> <FontVar> <data_prefix> [spacing] [--prop]')
        sys.exit(1)

    src, out_path, font_var, prefix = args[0], args[1], args[2], args[3]
    spacing = int(args[4]) if len(args) >= 5 else 0
    data_name = prefix.upper()

    glyphs, _ = parse_bdf(src)
    if not glyphs:
        print(f'Error: no glyphs in {src}')
        sys.exit(1)

    if '--prop' in flags:
        cells, cell_h, bpc = build_proportional(glyphs)
        if '--bmf' in flags:
            emit_bmf(out_path, cells, cell_h, bpc, spacing)
        else:
            emit_proportional(out_path, src, font_var, data_name, cells, cell_h, bpc, spacing)
    else:
        print('monospace mode removed from this version; use git history for v1. '
              'Pass --prop for proportional output.')
        sys.exit(1)


if __name__ == '__main__':
    main()
