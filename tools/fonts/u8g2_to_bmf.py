#!/usr/bin/env python3
"""
u8g2 font (C array format) → .bmf converter for Palanu font packs.

Usage:
  python3 u8g2_to_bmf.py <u8g2_fonts.c> <font_name> <out.bmf>

Example:
  python3 u8g2_to_bmf.py /tmp/u8g2_fonts.c u8g2_font_haxrcorp4089_tr \
      packs/Haxrcorp4089/reg8.bmf
"""

import sys
import re
import struct
import os

ASCII_FIRST = 0x20
ASCII_LAST  = 0x7E
HEADER_SIZE = 23   # U8G2_FONT_DATA_STRUCT_SIZE

# ── C string literal parser ───────────────────────────────────────────────────

def parse_c_string(s: str) -> bytes:
    s = re.sub(r'"\s*\n\s*"', '', s)
    s = s.strip().strip('"')
    out = []
    i = 0
    while i < len(s):
        if s[i] != '\\':
            out.append(ord(s[i])); i += 1; continue
        i += 1
        c = s[i]
        if c == 'n':    out.append(10);  i += 1
        elif c == 'r':  out.append(13);  i += 1
        elif c == 't':  out.append(9);   i += 1
        elif c == '\\': out.append(92);  i += 1
        elif c == '"':  out.append(34);  i += 1
        elif c == '0':  out.append(0);   i += 1
        elif c == 'x':
            out.append(int(s[i+1:i+3], 16)); i += 3
        elif c.isdigit():
            j = i
            while j < i+3 and j < len(s) and s[j].isdigit():
                j += 1
            out.append(int(s[i:j], 8) & 0xFF); i = j
        else:
            out.append(ord(c)); i += 1
    return bytes(out)

def extract_font_bytes(src_path: str, font_name: str) -> bytes:
    with open(src_path, 'r', errors='replace') as f:
        content = f.read()
    pattern = (r'const uint8_t ' + re.escape(font_name) +
               r'\[\d+\][^=]+=\s*\n?((?:"[^"]*"\s*\n?)+)')
    m = re.search(pattern, content)
    if not m:
        raise ValueError(f"Font '{font_name}' not found in {src_path}")
    return parse_c_string(m.group(1))

# ── u8g2 font header ──────────────────────────────────────────────────────────

class U8G2Header:
    def __init__(self, data: bytes):
        self.glyph_cnt            = data[0]
        self.bbx_mode             = data[1]
        self.bits_per_0           = data[2]   # RLE background run bits
        self.bits_per_1           = data[3]   # RLE foreground run bits
        self.bits_per_char_width  = data[4]
        self.bits_per_char_height = data[5]
        self.bits_per_char_x      = data[6]
        self.bits_per_char_y      = data[7]
        self.bits_per_delta_x     = data[8]   # advance width (signed)
        self.max_char_width       = data[9]
        self.max_char_height      = data[10]
        self.x_height             = struct.unpack_from('b', data, 11)[0]
        self.cap_height           = struct.unpack_from('b', data, 12)[0]
        self.ascent_A             = struct.unpack_from('b', data, 13)[0]
        self.descent_g            = struct.unpack_from('b', data, 14)[0]
        # Section offsets are RELATIVE to end of header (add HEADER_SIZE to get abs offset)
        self.start_pos_upper_A    = struct.unpack_from('>H', data, 17)[0]
        self.start_pos_lower_a    = struct.unpack_from('>H', data, 19)[0]
        self.start_pos_unicode    = struct.unpack_from('>H', data, 21)[0]

    def __repr__(self):
        return (f"U8G2Header(glyph_cnt={self.glyph_cnt}, "
                f"max={self.max_char_width}x{self.max_char_height}, "
                f"ascent_A={self.ascent_A}, descent_g={self.descent_g}, "
                f"bp0={self.bits_per_0} bp1={self.bits_per_1} "
                f"bpcw={self.bits_per_char_width} bpch={self.bits_per_char_height} "
                f"bpcx={self.bits_per_char_x} bpcy={self.bits_per_char_y} "
                f"bpdx={self.bits_per_delta_x})")

# ── Bit reader (LSB-first) ────────────────────────────────────────────────────

class BitReader:
    def __init__(self, data: bytes, start: int = 0):
        self.data = data
        self.byte_pos = start
        self.bit_pos = 0

    def read_unsigned(self, n: int) -> int:
        if n == 0:
            return 0
        val = 0
        for i in range(n):
            if self.byte_pos >= len(self.data):
                break
            bit = (self.data[self.byte_pos] >> self.bit_pos) & 1
            val |= (bit << i)
            self.bit_pos += 1
            if self.bit_pos >= 8:
                self.bit_pos = 0
                self.byte_pos += 1
        return val

    def read_signed(self, n: int) -> int:
        v = self.read_unsigned(n)
        v -= (1 << (n - 1))
        return v

# ── u8g2 glyph decoder ────────────────────────────────────────────────────────

def decode_glyph_bitmap(h: U8G2Header, br: BitReader, gw: int, gh: int) -> list:
    """Decode RLE bitmap. Returns list of rows (each row = list of bools)."""
    # Linear pixel array, row-major
    pixels = [False] * (gw * gh)
    px = 0   # x within row
    py = 0   # current row
    pos = 0  # linear position

    # u8g2 decode_glyph outer loop (from u8g2_font.c lines 692-704):
    #   for(;;) {
    #     a = read(bp0); b = read(bp1);
    #     do {
    #       draw(a bg); draw(b fg);
    #     } while( read(1) != 0 );
    #     if (y >= h) break;
    #   }
    while py < gh:
        a = br.read_unsigned(h.bits_per_0)  # background run
        b = br.read_unsigned(h.bits_per_1)  # foreground run
        while True:
            # Draw 'a' background pixels
            for _ in range(a):
                if py < gh:
                    pixels[py * gw + px] = False
                    px += 1
                    if px >= gw:
                        px = 0
                        py += 1
            # Draw 'b' foreground pixels
            for _ in range(b):
                if py < gh:
                    pixels[py * gw + px] = True
                    px += 1
                    if px >= gw:
                        px = 0
                        py += 1
            # Continuation bit
            if br.read_unsigned(1) == 0:
                break
        # Check if done (y >= h)
        if py >= gh:
            break

    # Reshape into rows
    rows = []
    for r in range(gh):
        rows.append(list(pixels[r * gw:(r + 1) * gw]))
    return rows

def decode_section(data: bytes, h: U8G2Header, section_start: int) -> dict:
    """Decode one glyph section.
    Entry format: [cp: 1 byte][size: 1 byte][glyph bit-stream: (size-2) bytes]
    End marker: size == 0.
    Returns {codepoint: (gw, gh, gx, gy, advance, rows)}.
    """
    glyphs = {}
    offset = section_start

    while offset < len(data):
        cp   = data[offset]
        size = data[offset + 1]
        if size == 0:
            break

        glyph_data_start = offset + 2
        glyph_data_end   = offset + size

        # Bit reader over this glyph's data bytes
        br = BitReader(data, glyph_data_start)

        gw  = br.read_unsigned(h.bits_per_char_width)
        gh  = br.read_unsigned(h.bits_per_char_height)
        gx  = br.read_signed(h.bits_per_char_x)
        gy  = br.read_signed(h.bits_per_char_y)
        adv = br.read_signed(h.bits_per_delta_x)

        rows = []
        if gw > 0 and gh > 0:
            rows = decode_glyph_bitmap(h, br, gw, gh)

        if ASCII_FIRST <= cp <= ASCII_LAST:
            glyphs[cp] = (gw, gh, gx, gy, adv, rows)

        offset += size

    return glyphs

def decode_u8g2_font(data: bytes) -> tuple:
    """Returns (header, glyphs_dict) where glyphs_dict maps cp → glyph tuple."""
    h = U8G2Header(data)
    print(h)

    all_glyphs = {}

    # Section offsets are relative to end of header → add HEADER_SIZE
    sections = [
        HEADER_SIZE + 0,                          # section 1: chars < 'A'
        HEADER_SIZE + h.start_pos_upper_A,        # section 2: 'A' range
        HEADER_SIZE + h.start_pos_lower_a,        # section 3: 'a' range
    ]

    for start in sections:
        if start < HEADER_SIZE or start >= len(data):
            continue
        g = decode_section(data, h, start)
        all_glyphs.update(g)

    return h, all_glyphs

# ── Render glyphs into shared cell and write .bmf ─────────────────────────────

def glyphs_to_bmf(h: U8G2Header, glyphs: dict, out_path: str, spacing: int = 0):
    # Compute shared baseline from decoded glyphs.
    # ascent = max pixels above baseline = max(gy + gh)
    # descent = max pixels below baseline = max(-gy) for negative gy
    valid = {cp: g for cp, g in glyphs.items() if g[1] > 0}
    if not valid:
        raise ValueError("No glyphs with height > 0 decoded")

    ascent  = max(g[3] + g[1] for g in valid.values())   # gy + gh
    descent = max(-g[3] for g in valid.values() if g[3] < 0) if any(g[3] < 0 for g in valid.values()) else 0
    cell_h  = ascent + descent
    bpc     = (cell_h + 7) // 8   # bytes per column

    widths  = []
    offsets = []
    data_bytes = []

    for cp in range(ASCII_FIRST, ASCII_LAST + 1):
        offsets.append(len(data_bytes))
        g = glyphs.get(cp)
        if g is None:
            widths.append(0)
            continue

        gw, gh, gx, gy, adv, rows = g
        advance = max(1, adv) if adv > 0 else max(1, gw)
        widths.append(advance)

        # Build column-major cell of size advance × cell_h
        cell = [[False] * cell_h for _ in range(advance)]

        if gw > 0 and gh > 0:
            # row_top = distance from cell top to glyph's first row
            row_top = ascent - (gy + gh)
            for r in range(gh):
                for c in range(gw):
                    if rows and r < len(rows) and c < len(rows[r]) and rows[r][c]:
                        cy = row_top + r
                        cx = gx + c
                        if 0 <= cy < cell_h and 0 <= cx < advance:
                            cell[cx][cy] = True

        # Encode column-major: bpc bytes per column
        for col in cell:
            for b in range(bpc):
                v = 0
                for bit in range(8):
                    row = b * 8 + bit
                    if row < cell_h and col[row]:
                        v |= (1 << bit)
                data_bytes.append(v)

    num     = ASCII_LAST - ASCII_FIRST + 1
    max_w   = max(widths) if widths else 0
    data_sz = len(data_bytes)

    header = struct.pack('<BBBBBBBBBBH',
        0xBF, 0x01, max_w, cell_h,
        ASCII_FIRST, num, spacing,
        bpc, 1, 1, data_sz)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(header)
        f.write(bytes(widths))
        for off in offsets:
            f.write(struct.pack('<H', off))
        f.write(bytes(data_bytes))

    total = len(header) + len(widths) + len(offsets) * 2 + data_sz
    print(f"  → {out_path}")
    print(f"     cell {max_w}×{cell_h}  ascent={ascent} descent={descent}  "
          f"{num} glyphs  {data_sz}B bitmap  {total}B total")

# ── ASCII art preview ─────────────────────────────────────────────────────────

def preview_glyphs(h: U8G2Header, glyphs: dict, chars: str = "AHImgpj!#"):
    valid = {cp: g for cp, g in glyphs.items() if g[1] > 0}
    if not valid:
        print("No glyphs to preview.")
        return
    ascent  = max(g[3] + g[1] for g in valid.values())
    descent = max(-g[3] for g in valid.values() if g[3] < 0) if any(g[3] < 0 for g in valid.values()) else 0
    cell_h  = ascent + descent

    for ch in chars:
        cp = ord(ch)
        if cp not in glyphs:
            continue
        gw, gh, gx, gy, adv, rows = glyphs[cp]
        advance = max(1, adv) if adv > 0 else max(1, gw)
        row_top = ascent - (gy + gh) if gh > 0 else 0
        cell = [['.'] * cell_h for _ in range(max(advance, 1))]
        for r in range(gh):
            for c in range(gw):
                if rows and r < len(rows) and c < len(rows[r]) and rows[r][c]:
                    cy = row_top + r
                    cx = gx + c
                    if 0 <= cy < cell_h and 0 <= cx < advance:
                        cell[cx][cy] = '#'
        print(f"'{ch}' cp=0x{cp:02X}  gw={gw} gh={gh} gx={gx} gy={gy} adv={adv}:")
        for row in range(cell_h):
            print('  ' + ''.join(cell[col][row] for col in range(advance)))
        print()

# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) == 4:
        src_path, font_name, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    elif len(sys.argv) == 3:
        src_path, font_name = sys.argv[1], sys.argv[2]
        out_path = None
    else:
        print(__doc__)
        sys.exit(1)

    data = extract_font_bytes(src_path, font_name)
    h, glyphs = decode_u8g2_font(data)
    print(f"Decoded {len(glyphs)} glyphs in ASCII range")
    preview_glyphs(h, glyphs)

    if out_path:
        glyphs_to_bmf(h, glyphs, out_path)

if __name__ == '__main__':
    main()
