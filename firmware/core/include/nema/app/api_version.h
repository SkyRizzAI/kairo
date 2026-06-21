#pragma once
#include <cstdint>

// Nema System API version — packed u16.u16 (semver for the app surface).
//
// Semantics (enforced at app-load time, Plan 48 Fase 3):
//   - major must match EXACTLY between app and host  (incompatible ABI)
//   - app.minor ≤ host.minor                         (app must not require
//     functions that don't exist yet)
//   - host.minor > app.minor is OK                   (host is newer;
//     backward-compatible)
//
// Bump rules:
//   - Add optional interface/function/field → minor++
//   - Remove, rename, or change signature → major++
//
// Pattern: Flipper Zero api_version u16.u16 (application_manifest.h:27–35).
// Representasi final di manifest .papp → Plan 59 (.papp packaging).

namespace nema {

inline constexpr uint16_t NEMA_API_VERSION_MAJOR = 1;
inline constexpr uint16_t NEMA_API_VERSION_MINOR = 0;

/// Packed u16.u16 for fast integer comparison.
inline constexpr uint32_t NEMA_API_VERSION =
    (static_cast<uint32_t>(NEMA_API_VERSION_MAJOR) << 16) |
     static_cast<uint32_t>(NEMA_API_VERSION_MINOR);

/// Unpack major from a packed version.
inline constexpr uint16_t apiVersionMajor(uint32_t packed) {
    return static_cast<uint16_t>(packed >> 16);
}

/// Unpack minor from a packed version.
inline constexpr uint16_t apiVersionMinor(uint32_t packed) {
    return static_cast<uint16_t>(packed & 0xFFFF);
}

/// Check whether an app built against `appVersion` is compatible with this host.
inline constexpr bool isApiCompatible(uint32_t appVersion) {
    if (apiVersionMajor(appVersion) != NEMA_API_VERSION_MAJOR) return false;
    if (apiVersionMinor(appVersion) > NEMA_API_VERSION_MINOR) return false;
    return true;
}

} // namespace nema
