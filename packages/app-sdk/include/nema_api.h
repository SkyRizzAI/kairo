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

// ── Process argv (Plan 86) ────────────────────────────────────────────────────
// Apps can read their argv (set from manifest.args or from the CLI call).
// argv[0] is always the app bundle id.

NEMA_IMPORT("nema", "argc")
extern int nema_argc(void);

// Copy argv[i] into buf (NUL-terminated), up to cap bytes.
// Returns bytes written (excluding NUL), or -1 if i is out of range.
NEMA_IMPORT("nema", "argv_get")
extern int nema_argv_get(int i, char* buf, int cap);

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

// ── Raw canvas ABI (Plan 86 Fase 2) ──────────────────────────────────────────
// Drawing calls flip the host into Gui mode on first use (irreversible per run).
// canvas_flush() publishes the drawn frame — call at the end of each frame.
//
// Color: 0 = background (black on mono displays), 1 = foreground (white/lit).
// All coordinates are LOGICAL pixels. Use canvas_width()/canvas_height() — never
// hardcode display dimensions.

#define COLOR_BLACK 0
#define COLOR_WHITE 1
#define COLOR_BG    0
#define COLOR_FG    1

NEMA_IMPORT("canvas", "canvas_width")
extern int canvas_width(void);

NEMA_IMPORT("canvas", "canvas_height")
extern int canvas_height(void);

// Fill the entire canvas with color (0=clear, 1=fill).
NEMA_IMPORT("canvas", "canvas_clear")
extern void canvas_clear(int color);

// Set one pixel at (x, y).
NEMA_IMPORT("canvas", "canvas_pixel")
extern void canvas_pixel(int x, int y, int color);

// Filled rectangle at (x, y) with width w and height h.
NEMA_IMPORT("canvas", "canvas_fill_rect")
extern void canvas_fill_rect(int x, int y, int w, int h, int color);

// Outline rectangle (1px border) at (x, y) with width w and height h.
NEMA_IMPORT("canvas", "canvas_rect")
extern void canvas_rect(int x, int y, int w, int h, int color);

// Draw a line from (x0, y0) to (x1, y1).
NEMA_IMPORT("canvas", "canvas_line")
extern void canvas_line(int x0, int y0, int x1, int y1, int color);

// Draw NUL-terminated text string at (x, y).
NEMA_IMPORT("canvas", "canvas_text")
extern void canvas_text(int x, int y, const char* msg, int color);

// Publish the current frame. Call after drawing is complete.
NEMA_IMPORT("canvas", "canvas_flush")
extern void canvas_flush(void);

// ── Retained UI ABI (Plan 86 Fase 3) ─────────────────────────────────────────
// Declarative retained-mode UI. App owns the event loop — NO host→guest callbacks.
//
// Usage pattern:
//   while (1) {
//       ui_begin();
//       ui_title("My App");
//       ui_text("Count: 5");
//       ui_button("Inc", 1);   // id=1
//       ui_button("Dec", 2);   // id=2
//       ui_end();              // renders frame; host handles focus highlight
//       int ev = ui_wait_event();
//       if (ev == EV_BACK) break;
//       if (ev == 1) count++;
//       if (ev == 2) count--;
//   }
//
// ui_begin/end flip the host to Gui mode on first call (same as canvas_*).
// Button ids must be > 0 (positive integers of your choice).

#define EV_NONE  0    // ui_poll_event: no event ready
#define EV_BACK -1    // Back/Cancel pressed

NEMA_IMPORT("ui", "ui_begin")
extern void ui_begin(void);

NEMA_IMPORT("ui", "ui_title")
extern void ui_title(const char* msg);

NEMA_IMPORT("ui", "ui_text")
extern void ui_text(const char* msg);

// Add a focusable button. id must be > 0; returned by ui_wait_event on activate.
NEMA_IMPORT("ui", "ui_button")
extern void ui_button(const char* label, int id);

NEMA_IMPORT("ui", "ui_row_begin")
extern void ui_row_begin(void);

NEMA_IMPORT("ui", "ui_row_end")
extern void ui_row_end(void);

NEMA_IMPORT("ui", "ui_col_begin")
extern void ui_col_begin(void);

NEMA_IMPORT("ui", "ui_col_end")
extern void ui_col_end(void);

// Commit the frame and render it. Call after all ui_* element calls.
NEMA_IMPORT("ui", "ui_end")
extern void ui_end(void);

// Block until an event: returns button id (>0), EV_BACK (-1).
NEMA_IMPORT("ui", "ui_wait_event")
extern int ui_wait_event(void);

// Non-blocking poll: returns button id, EV_BACK, or EV_NONE (0).
NEMA_IMPORT("ui", "ui_poll_event")
extern int ui_poll_event(void);

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
