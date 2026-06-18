#pragma once

// Canonical capability catalog (Plan 42).
//
// Capability identity stays a STRING (flexible, forward-compatible, JS-friendly:
// a board may declare a capability the core has never heard of). But every
// producer and consumer references these constants instead of scattering string
// literals — so typos and duplicates are caught, and the taxonomy lives in one
// documented place.
//
// Rule of thumb for the namespace:
//   <domain>            e.g. "display", "camera", "storage"
//   <domain>.<detail>   e.g. "input.touch", "audio.output", "net.wifi"
//
// `has(cap)`  → static: "this box was built able to do X" (never changes).
// `available(cap)` → dynamic: "X is up and usable right now" (Plan 42 liveness).

namespace nema::caps {

// Display & input
inline constexpr const char* Display       = "display";
inline constexpr const char* Input         = "input";
inline constexpr const char* InputPrev     = "input.prev";
inline constexpr const char* InputNext     = "input.next";
inline constexpr const char* InputActivate = "input.activate";
inline constexpr const char* InputBack     = "input.back";
inline constexpr const char* InputAdjust   = "input.adjust";
inline constexpr const char* Input2D       = "input.2d";
inline constexpr const char* InputTouch    = "input.touch";

// Media
inline constexpr const char* Camera        = "camera";
inline constexpr const char* AudioInput    = "audio.input";
inline constexpr const char* AudioOutput   = "audio.output";
inline constexpr const char* Rgb           = "rgb";

// Sensors
inline constexpr const char* SensorsEnv    = "sensors.environment";
inline constexpr const char* SensorsLight  = "sensors.light";
inline constexpr const char* SensorsMotion = "sensors.motion";

// Net & connectivity (taxonomy tidied: net.* replaces scattered wifi/networking/http)
inline constexpr const char* NetWifi       = "net.wifi";
inline constexpr const char* NetHttp       = "net.http";
    inline constexpr const char* BtBle         = "bt.ble";
    inline constexpr const char* BtBleCentral  = "bt.ble.central";   // Plan 67
    inline constexpr const char* UsbHid         = "usb.hid";          // Plan 66 — BLE scanner

// System
inline constexpr const char* Storage       = "storage";
inline constexpr const char* RemoteUsb     = "remote.usb";
inline constexpr const char* Profile       = "profile";

} // namespace nema::caps
