// nema_api.h — Palanu WASM App SDK (Plan 85)
//
// Single header for WASM apps. Include ONLY this — do NOT include <stdio.h>,
// <string.h>, or any other stdlib/WASI header. Every function listed here is
// provided by the Palanu firmware at runtime via the wasm3 host-import table.
//
// Compile flags (handled by `bun run app:build`):
//   --target=wasm32-unknown-unknown -nostdlib -O2
//   -Wl,--no-entry -Wl,--export=main
//   -Wl,--allow-undefined -Wl,--strip-all
//
// Usage:
//   #include "nema_api.h"
//   NEMA_EXPORT int main(void) { nema_print("hello\n"); return 0; }

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ── Compiler attributes ────────────────────────────────────────────────────────
#ifdef __wasm__
#  define NEMA_IMPORT(mod, name) \
     __attribute__((import_module(mod), import_name(name)))
// Mark a function as exported to the wasm3 host.
// REQUIRED on main(): export_name("X") is the wasm-native way to export a
// symbol — it both prevents DCE and adds the name to the wasm export table.
// visibility("default") alone does NOT create a wasm export entry.
#  define NEMA_EXPORT __attribute__((export_name("main")))
#else
#  define NEMA_IMPORT(mod, name)
#  define NEMA_EXPORT
#endif

// ── Logging ───────────────────────────────────────────────────────────────────
// Levels: "trace" "debug" "info" "warn" "error" "fatal"

NEMA_IMPORT("nema", "log")
extern void nema_log(const char* level, const char* tag, const char* msg);

// Convenience: print a line to the firmware log (info level, tag = app id).
// Use this instead of printf() for CLI output — routes to the log stdout sink.
NEMA_IMPORT("nema", "print")
extern void nema_print(const char* msg);

// ── Device info ───────────────────────────────────────────────────────────────
// Write device name/caps into caller-supplied buffer. Returns bytes written
// (excluding NUL), or -1 on error. Caps are newline-separated.

NEMA_IMPORT("nema", "device_name")
extern int nema_device_name(char* out, int cap);

NEMA_IMPORT("nema", "device_caps")
extern int nema_device_caps(char* out, int cap);

// ── Storage — namespaced file I/O ────────────────────────────────────────────
// Paths are relative filenames ("count.txt", "cache/data.bin").
// Routed to /data/<bundle-id>/ automatically — no manual path construction.

// Read file into buf. Returns bytes written, -1 if file not found.
NEMA_IMPORT("nema", "storage_fs_read_file")
extern int nema_storage_fs_read_file(const char* name, char* out, int cap);

// Write data to file. Returns 0 on success, -1 on failure.
NEMA_IMPORT("nema", "storage_fs_write_file")
extern int nema_storage_fs_write_file(const char* name, const char* data, int len);

// ── Minimal libc substitutes ─────────────────────────────────────────────────
// Basic string/number utilities that do NOT pull in WASI libc.
// Only what WASM apps actually need.

static inline int nema_strlen(const char* s) {
    int n = 0; while (s[n]) n++; return n;
}

// Write integer n into buf (at least 12 bytes). Returns pointer to buf.
static inline char* nema_itoa(int n, char* buf) {
    char tmp[12]; int i = 0;
    if (n < 0) { buf[0] = '-'; n = -n; char* r = nema_itoa(n, buf + 1); (void)r; return buf; }
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return buf; }
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0; while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0'; return buf;
}

// Minimal atoi (no negative, no whitespace skip needed for our use cases)
static inline int nema_atoi(const char* s) {
    int n = 0;
    if (s && *s == '-') return 0;  // negative not needed
    while (s && *s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return n;
}

// memcpy / memset without libc
static inline void* nema_memcpy(void* dst, const void* src, int n) {
    char* d = (char*)dst; const char* s = (const char*)src;
    while (n--) *d++ = *s++; return dst;
}
static inline void* nema_memset(void* dst, int c, int n) {
    char* d = (char*)dst; while (n--) *d++ = (char)c; return dst;
}

#ifdef __cplusplus
}
#endif
