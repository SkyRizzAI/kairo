#pragma once
#include <string>
#include <cstddef>

// Portable SHA-256 (FIPS 180-4). No platform dependencies — runs on host,
// WASM, and ESP32 identically. Used by ProfileService for password hashing.
namespace nema {

// SHA-256 of `data`, returned as 64-char lowercase hex string.
std::string hexSha256(const std::string& data);

// XorShift32 PRNG salt — `n` random bytes returned as 2n hex chars.
// Not cryptographically secure, but sufficient for rainbow-table defence.
// Each call advances internal state; seed is derived from the static
// variable's load address (differs per binary / device).
std::string randomHexSalt(size_t n = 16);

} // namespace nema
