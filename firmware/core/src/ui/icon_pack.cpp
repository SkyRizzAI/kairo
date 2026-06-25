// Plan 53 — Built-in 8×8 icon bitmaps.
// Each bitmap is 8 bytes (1 byte per row, MSB = leftmost pixel).
// Handles: status.{wifi,bt,battery,charging}
//          feature.{apps,settings,gpio,subghz,nfc}
//          file.{folder,file,generic}
//          action.{warning,info,ok,spinner}
#include "nema/ui/icon_pack.h"
#include <cstring>

namespace aether::ui {

// ── status.wifi ───────────────────────────────────────────────────────────────
static const uint8_t k_wifi[] = {
    0b00000000,
    0b00111100,
    0b01000010,
    0b00011000,
    0b00100100,
    0b00001000,
    0b00011000,
    0b00000000,
};

// ── status.bt ─────────────────────────────────────────────────────────────────
static const uint8_t k_bt[] = {
    0b00001000,
    0b00001100,
    0b00101010,
    0b00011000,
    0b00011000,
    0b00101010,
    0b00001100,
    0b00001000,
};

// ── status.battery ────────────────────────────────────────────────────────────
static const uint8_t k_battery[] = {
    0b00000000,
    0b01111110,
    0b01000010,
    0b01011010,
    0b01011010,
    0b01000010,
    0b01111110,
    0b00000000,
};

// ── status.charging ───────────────────────────────────────────────────────────
static const uint8_t k_charging[] = {
    0b00000000,
    0b01111110,
    0b01011010,
    0b01111110,
    0b01011010,
    0b01111110,
    0b01111110,
    0b00000000,
};

// ── feature.apps ──────────────────────────────────────────────────────────────
static const uint8_t k_apps[] = {
    0b11011011,
    0b11011011,
    0b00000000,
    0b11011011,
    0b11011011,
    0b00000000,
    0b11011011,
    0b11011011,
};

// ── feature.settings ──────────────────────────────────────────────────────────
static const uint8_t k_settings[] = {
    0b00011000,
    0b00111100,
    0b01100110,
    0b11111111,
    0b11111111,
    0b01100110,
    0b00111100,
    0b00011000,
};

// ── feature.gpio ──────────────────────────────────────────────────────────────
static const uint8_t k_gpio[] = {
    0b10101010,
    0b10101010,
    0b11111110,
    0b10000010,
    0b10111010,
    0b10000010,
    0b11111110,
    0b00000000,
};

// ── feature.subghz ────────────────────────────────────────────────────────────
static const uint8_t k_subghz[] = {
    0b00000000,
    0b01111110,
    0b10000001,
    0b00111100,
    0b01000010,
    0b00011000,
    0b00011000,
    0b00000000,
};

// ── feature.nfc ───────────────────────────────────────────────────────────────
static const uint8_t k_nfc[] = {
    0b00000000,
    0b00111100,
    0b01000010,
    0b10011001,
    0b10011001,
    0b01000010,
    0b00111100,
    0b00000000,
};

// ── file.folder ───────────────────────────────────────────────────────────────
static const uint8_t k_folder[] = {
    0b00000000,
    0b01110000,
    0b11111111,
    0b10000001,
    0b10000001,
    0b10000001,
    0b11111111,
    0b00000000,
};

// ── file.file ─────────────────────────────────────────────────────────────────
static const uint8_t k_file[] = {
    0b00111110,
    0b00101010,
    0b00111110,
    0b00100010,
    0b00100010,
    0b00100010,
    0b00111110,
    0b00000000,
};

// ── file.generic ──────────────────────────────────────────────────────────────
static const uint8_t k_generic[] = {
    0b01111100,
    0b01000100,
    0b01110100,
    0b01000100,
    0b01000100,
    0b01000100,
    0b01111100,
    0b00000000,
};

// ── action.warning ────────────────────────────────────────────────────────────
static const uint8_t k_warning[] = {
    0b00010000,
    0b00111000,
    0b01101100,
    0b11000110,
    0b11111110,
    0b00000000,
    0b00111000,
    0b00000000,
};

// ── action.info ───────────────────────────────────────────────────────────────
static const uint8_t k_info[] = {
    0b00011000,
    0b00011000,
    0b00000000,
    0b00111000,
    0b00011000,
    0b00011000,
    0b00111100,
    0b00000000,
};

// ── action.ok ─────────────────────────────────────────────────────────────────
static const uint8_t k_ok[] = {
    0b00000000,
    0b00000001,
    0b00000011,
    0b00000110,
    0b01001100,
    0b01111000,
    0b00110000,
    0b00000000,
};

// ── action.spinner ────────────────────────────────────────────────────────────
static const uint8_t k_spinner[] = {
    0b00111000,
    0b01000100,
    0b10000010,
    0b10000000,
    0b10000010,
    0b01000100,
    0b00111000,
    0b00010000,
};

// ── nav.up (6×6 chevron, vertically centred) ──────────────────────────────────
// 6×6, continuous bit-packing (row*6+col, MSB first) — verified via scratchpad
// encoder. Compact footer-legend variant (smaller than the 8×8 pack icons).
//   ......  ......  ..##..  .####.  ##..##  ......
static const uint8_t k_navup[] = { 0x00, 0x03, 0x1e, 0xcc, 0x00 };

// ── nav.enter (6×6 return arrow ↵: riser down-right, arrowhead left) ───────────
//   ....#.  ....#.  .#..#.  #####.  .#....  ......
static const uint8_t k_enter[] = { 0x08, 0x24, 0xbe, 0x40, 0x00 };

// ── action.dot (filled disc) ──────────────────────────────────────────────────
static const uint8_t k_dot[] = {
    0b00000000,
    0b00111100,
    0b01111110,
    0b01111110,
    0b01111110,
    0b01111110,
    0b00111100,
    0b00000000,
};

// ── feature.wallet (billfold + snap-button clasp) ─────────────────────────────
static const uint8_t k_wallet[] = {
    0b00000000,   // ........
    0b01111110,   // .######.
    0b01000010,   // .#....#.
    0b01000010,   // .#....#.
    0b01000110,   // .#...##.
    0b01000110,   // .#...##.
    0b01111110,   // .######.
    0b00000000,   // ........
};

// ── Table ─────────────────────────────────────────────────────────────────────
static const IconDef k_icons[] = {
    { "status.wifi",      k_wifi,     8, 8 },
    { "status.bt",        k_bt,       8, 8 },
    { "status.battery",   k_battery,  8, 8 },
    { "status.charging",  k_charging, 8, 8 },
    { "feature.apps",     k_apps,     8, 8 },
    { "feature.wallet",   k_wallet,   8, 8 },
    { "feature.settings", k_settings, 8, 8 },
    { "feature.gpio",     k_gpio,     8, 8 },
    { "feature.subghz",   k_subghz,   8, 8 },
    { "feature.nfc",      k_nfc,      8, 8 },
    { "file.folder",      k_folder,   8, 8 },
    { "file.file",        k_file,     8, 8 },
    { "file.generic",     k_generic,  8, 8 },
    { "action.warning",   k_warning,  8, 8 },
    { "action.info",      k_info,     8, 8 },
    { "action.ok",        k_ok,       8, 8 },
    { "action.spinner",   k_spinner,  8, 8 },
    { "action.dot",       k_dot,      8, 8 },
    { "nav.up",           k_navup,    6, 6 },
    { "nav.enter",        k_enter,    6, 6 },
    { nullptr, nullptr, 0, 0 },
};

static const IconDef* k_iconPtrs[] = {
    &k_icons[ 0], &k_icons[ 1], &k_icons[ 2], &k_icons[ 3],
    &k_icons[ 4], &k_icons[ 5], &k_icons[ 6], &k_icons[ 7],
    &k_icons[ 8], &k_icons[ 9], &k_icons[10], &k_icons[11],
    &k_icons[12], &k_icons[13], &k_icons[14], &k_icons[15],
    &k_icons[16], &k_icons[17], &k_icons[18], &k_icons[19],
    nullptr,
};

const IconDef* findIcon(const char* handle) {
    if (!handle) return nullptr;
    for (const IconDef* d = k_icons; d->handle; ++d)
        if (strcmp(d->handle, handle) == 0) return d;
    return nullptr;
}

const IconDef* const* allIcons() { return k_iconPtrs; }

} // namespace aether::ui
