// Sys Info — WASM/C version (Plan 84 Fase 4).
// CLI mode: print device name, capabilities, dan uptime ke stdout.
//
// Build: wasi-sdk clang --target=wasm32-wasi -o sysinfo.wasm main.c
// Usage: sysinfo-wasm [--json]

#include <stdio.h>
#include <string.h>

#ifdef __wasm__
#  define NEMA_IMPORT(mod, name) __attribute__((import_module(mod), import_name(name)))
#else
#  define NEMA_IMPORT(mod, name)
#endif

NEMA_IMPORT("nema", "device_name")
extern int nema_device_name(char* out, int len);

NEMA_IMPORT("nema", "device_caps")
extern int nema_device_caps(char* out, int len);  // newline-separated list

NEMA_IMPORT("nema", "log")
extern void nema_log(const char* level, const char* tag, const char* msg);

int main(int argc, char* argv[]) {
    int json = (argc > 1 && strcmp(argv[1], "--json") == 0);

    char name[64] = {0};
    nema_device_name(name, sizeof(name));

    char caps[512] = {0};
    nema_device_caps(caps, sizeof(caps));

    if (json) {
        printf("{\"device\":\"%s\",\"caps\":[", name);
        int first = 1;
        char* tok = strtok(caps, "\n");
        while (tok) {
            if (!first) printf(",");
            printf("\"%s\"", tok);
            first = 0;
            tok = strtok(NULL, "\n");
        }
        printf("]}\n");
    } else {
        printf("Device: %s\n", name);
        printf("Capabilities:\n");
        char* tok = strtok(caps, "\n");
        while (tok) {
            printf("  - %s\n", tok);
            tok = strtok(NULL, "\n");
        }
    }

    nema_log("info", "sysinfo-wasm", "done");
    return 0;
}
