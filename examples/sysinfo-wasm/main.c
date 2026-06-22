// Sys Info — WASM bare-metal (Plan 85)
// Print device name and capabilities. No stdio.h — all I/O via nema_api.h.
//
// Build: bun run app:build:sysinfo-wasm
// Usage: sysinfo-wasm

#include "nema_api.h"

NEMA_EXPORT int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    char name[64];
    nema_memset(name, 0, sizeof(name));
    nema_device_name(name, sizeof(name));

    char msg[128];
    // "Device: <name>"
    int i = 0, j = 0;
    const char* prefix = "Device: ";
    while (prefix[i]) msg[j++] = prefix[i++];
    i = 0; while (name[i]) msg[j++] = name[i++];
    msg[j] = '\0';
    nema_print(msg);

    // Print each capability on its own line
    char caps[512];
    nema_memset(caps, 0, sizeof(caps));
    int n = nema_device_caps(caps, sizeof(caps) - 1);
    if (n > 0) {
        nema_print("Capabilities:");
        // Walk newline-separated list
        char line[64];
        int ci = 0, li = 0;
        while (ci <= n) {
            char ch = (ci < n) ? caps[ci] : '\n';
            if (ch == '\n' || ch == '\0') {
                if (li > 0) {
                    line[li] = '\0';
                    char out[72] = "  ";
                    int oi = 2, k = 0;
                    while (line[k]) out[oi++] = line[k++];
                    out[oi] = '\0';
                    nema_print(out);
                }
                li = 0;
            } else if (li < (int)sizeof(line) - 1) {
                line[li++] = ch;
            }
            ci++;
        }
    }

    nema_log("info", "sysinfo-wasm", "done");
    return 0;
}
