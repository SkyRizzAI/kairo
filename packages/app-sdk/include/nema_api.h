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

// ── Input + timing ABI (Plan 86 Fase 4) ──────────────────────────────────────
// For raw-canvas apps that manage their own render loop.
// Retained-UI apps should use ui_wait_event() instead.
//
// Only Press events are returned; Release and Repeat are filtered by the host.
// The host maps physical keys → Action via the board's keymap (no hardcoded keys).

// Action constants — same values as input::Action enum in firmware.
#define ACT_NONE     0   // no action / timeout
#define ACT_PREV     1   // navigate backward  (Prev button)
#define ACT_NEXT     2   // navigate forward   (Next button)
#define ACT_ACTIVATE 3   // confirm / select   (Activate button)
#define ACT_BACK     4   // go back / escape   (Back button)
#define ACT_UP       5   // AdjustUp   (optional; may be absent on some boards)
#define ACT_DOWN     6   // AdjustDown (optional; may be absent on some boards)

// Non-blocking poll. Returns ACT_* constant, or ACT_NONE if mailbox is empty.
NEMA_IMPORT("input", "input_poll")
extern int input_poll(void);

// Block until input arrives or timeout_ms elapses.
// Returns ACT_* constant, or ACT_NONE on timeout. Pass 0 to wait forever.
NEMA_IMPORT("input", "input_wait")
extern int input_wait(int timeout_ms);

// Yield the app thread for ms milliseconds. The GUI thread stays responsive.
NEMA_IMPORT("input", "delay")
extern void delay(int ms);

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

static inline int nema_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
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

// ── printf shim (Plan 86 Fase 5) ──────────────────────────────────────────────
// Minimal varargs printf → buffer → nema_print. No WASI libc needed.
// Supports: %d %i %u %x %X %s %c %% (no width/precision beyond basic %0d).
// Buffer is 256 bytes. Long strings are truncated at 255 chars + NUL.
// Use this instead of stdio printf which requires WASI libc linkage.

#ifndef NEMA_NO_PRINTF

#ifdef __cplusplus
#include <cstdarg>
#else
#include <stdarg.h>
#endif

static inline void printf(const char* fmt, ...) {
    char buf[256];
    int  out = 0;
    va_list ap;
    va_start(ap, fmt);
    for (const char* p = fmt; *p && out < 254; p++) {
        if (*p != '%') { buf[out++] = *p; continue; }
        p++;
        if (*p == '\0') break;
        if (*p == '%') { buf[out++] = '%'; continue; }
        if (*p == 'c') {
            buf[out++] = (char)va_arg(ap, int);
            continue;
        }
        if (*p == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s && out < 254) buf[out++] = *s++;
            continue;
        }
        // Numeric: handle optional leading zero + width digit before specifier
        int zero = 0, width = 0;
        if (*p == '0') { zero = 1; p++; }
        while (*p >= '1' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        if (*p == 'd' || *p == 'i') {
            int v = va_arg(ap, int);
            char tmp[12]; char* t = tmp + 12; *--t = '\0';
            int neg = (v < 0); unsigned uv = neg ? (unsigned)(-v) : (unsigned)v;
            if (uv == 0) *--t = '0';
            while (uv) { *--t = '0' + (uv % 10); uv /= 10; }
            if (neg) *--t = '-';
            int len = (int)(tmp + 11 - t);
            if (width > len) {
                char pad = zero ? '0' : ' ';
                while (width-- > len && out < 254) buf[out++] = pad;
            }
            while (*t && out < 254) buf[out++] = *t++;
        } else if (*p == 'u') {
            unsigned v = va_arg(ap, unsigned);
            char tmp[12]; char* t = tmp + 12; *--t = '\0';
            if (v == 0) *--t = '0';
            while (v) { *--t = '0' + (v % 10); v /= 10; }
            int len = (int)(tmp + 11 - t);
            if (width > len) {
                char pad = zero ? '0' : ' ';
                while (width-- > len && out < 254) buf[out++] = pad;
            }
            while (*t && out < 254) buf[out++] = *t++;
        } else if (*p == 'x' || *p == 'X') {
            unsigned v = va_arg(ap, unsigned);
            const char* hex = (*p == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
            char tmp[12]; char* t = tmp + 12; *--t = '\0';
            if (v == 0) *--t = '0';
            while (v) { *--t = hex[v & 0xF]; v >>= 4; }
            int len = (int)(tmp + 11 - t);
            if (width > len) {
                char pad = zero ? '0' : ' ';
                while (width-- > len && out < 254) buf[out++] = pad;
            }
            while (*t && out < 254) buf[out++] = *t++;
        } else {
            buf[out++] = '%'; buf[out++] = *p;  // pass through unknown
        }
    }
    va_end(ap);
    buf[out] = '\0';
    nema_print(buf);
}

#endif // NEMA_NO_PRINTF

// ── display_* ergonomic aliases (Plan 86 Fase 5) ──────────────────────────────
// AkiraOS-style short names. Map directly to canvas_* — zero overhead.
#define display_width()              canvas_width()
#define display_height()             canvas_height()
#define display_clear(color)         canvas_clear(color)
#define display_pixel(x,y,c)         canvas_pixel(x,y,c)
#define display_fill_rect(x,y,w,h,c) canvas_fill_rect(x,y,w,h,c)
#define display_rect(x,y,w,h,c)      canvas_rect(x,y,w,h,c)
#define display_line(x0,y0,x1,y1,c)  canvas_line(x0,y0,x1,y1,c)
#define display_text(x,y,msg,c)       canvas_text(x,y,msg,c)
#define display_flush()              canvas_flush()

#ifdef __cplusplus
}
#endif
