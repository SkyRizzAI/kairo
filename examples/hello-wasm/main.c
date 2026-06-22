// Hello — WASM bare-metal (Plan 85)
// Greet and print device info. No stdio.h — all I/O via nema_api.h.
//
// Build: bun run app:build:hello-wasm
// Usage: hello-wasm [name]

#include "nema_api.h"

static void strcat2(char* dst, int cap, const char* a, const char* b) {
    int i = 0;
    while (*a && i < cap - 1) dst[i++] = *a++;
    while (*b && i < cap - 1) dst[i++] = *b++;
    dst[i] = '\0';
}

NEMA_EXPORT int main(int argc, char* argv[]) {
    char device[64];
    nema_memset(device, 0, sizeof(device));
    nema_device_name(device, sizeof(device));

    const char* who = (argc > 1) ? argv[1] : "World";

    char msg[128];
    strcat2(msg, sizeof(msg), "Hello, ", who);
    nema_print(msg);

    char info[128];
    strcat2(info, sizeof(info), "Running on ", device);
    nema_print(info);

    nema_log("info", "hello-wasm", "done");
    return 0;
}
