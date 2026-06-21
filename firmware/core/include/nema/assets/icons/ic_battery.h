// Battery outline 16×8 px (body 14-wide + 2-px nub on right, rows 2-5).
// Proportional to icWifiOn (10×8) — 1.6× instead of the old 2.5× (25×8).
static const uint8_t kIcBattery[] = {
    0xFF, 0xFC, // row 0: top border (cols 0-13 ON)
    0x80, 0x04, // row 1: side borders
    0x80, 0x07, // row 2: side borders + nub (cols 14-15)
    0x80, 0x07, // row 3
    0x80, 0x07, // row 4
    0x80, 0x07, // row 5
    0x80, 0x04, // row 6: side borders
    0xFF, 0xFC, // row 7: bottom border
};
