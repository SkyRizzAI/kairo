#pragma once
// T1 System Icons — converted from Flipper Zero assets by tools/asset_gen/png2c.
// Bit encoding: 1=pixel ON, MSB-first, non-byte-aligned row-major.
// (Canvas::drawBitmap reads: bitIdx = row*w + col, bit = bits[bitIdx/8] >> (7-(bitIdx%8)) & 1)
//
// Missing icons (no Flipper source, hand-coded placeholders):
//   kIcWifi* — Flipper has no WiFi hardware. 10×8 placeholder.
#include <cstdint>

namespace nema::assets {

struct Icon { uint8_t w; uint8_t h; const uint8_t* data; };

// ── Raw bitmap data ─────────────────────────────────────────────────────────

#include "icons/ic_battery.h"
#include "icons/ic_charging.h"
#include "icons/ic_ble_idle.h"
#include "icons/ic_ble_connected.h"
#include "icons/ic_ble_beacon.h"
#include "icons/ic_sd_mounted.h"
#include "icons/ic_sd_fail.h"
#include "icons/ic_lock.h"
#include "icons/ic_arrow_up.h"
#include "icons/ic_arrow_down.h"
#include "icons/ic_arrow_left.h"
#include "icons/ic_arrow_right.h"

// WiFi placeholder — hand-coded 10×8, concentric arcs pointing up
// TODO: replace with a real icon once WiFi hardware assets are designed.
static const uint8_t kIcWifiOn[] = {
    0x00, 0x0F, 0xC4, 0x08, 0x78, 0x21, 0x03, 0x00, 0x00, 0x20
};
static const uint8_t kIcWifiOff[] = {
    // Same shape with a slash (rows 2 and 5 flipped to show disconnected)
    0x00, 0x0F, 0x84, 0x48, 0x54, 0x29, 0x03, 0x00, 0x00, 0x20
};

// ── Named icon descriptors ───────────────────────────────────────────────────

inline constexpr Icon icBattery      = { 25,  8, kIcBattery      };
inline constexpr Icon icCharging     = {  9, 10, kIcCharging      };
inline constexpr Icon icBleIdle      = {  5,  8, kIcBleIdle       };
inline constexpr Icon icBleConnected = { 16,  8, kIcBleConnected  };
inline constexpr Icon icBleBeacon    = {  7,  8, kIcBleBeacon     };
inline constexpr Icon icSdMounted    = { 11,  8, kIcSdMounted     };
inline constexpr Icon icSdFail       = { 11,  8, kIcSdFail        };
inline constexpr Icon icLock         = {  9,  8, kIcLock          };
inline constexpr Icon icArrowUp      = {  5,  3, kIcArrowUp       };
inline constexpr Icon icArrowDown    = {  5,  3, kIcArrowDown     };
inline constexpr Icon icArrowLeft    = {  3,  5, kIcArrowLeft     };
inline constexpr Icon icArrowRight   = {  3,  5, kIcArrowRight    };
inline constexpr Icon icWifiOn       = { 10,  8, kIcWifiOn        };
inline constexpr Icon icWifiOff      = { 10,  8, kIcWifiOff       };

} // namespace nema::assets
