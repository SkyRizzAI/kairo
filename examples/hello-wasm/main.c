// Hello — WASM/C version (Plan 84 Fase 4).
// Minimal example: greet + log. Entry point untuk template WASM baru.
//
// Build: wasi-sdk clang --target=wasm32-wasi -o hello.wasm main.c
// Usage: hello-wasm [name]

#include <stdio.h>
#include <string.h>

#ifdef __wasm__
#  define NEMA_IMPORT(mod, name) __attribute__((import_module(mod), import_name(name)))
#else
#  define NEMA_IMPORT(mod, name)
#endif

NEMA_IMPORT("nema", "device_name")
extern int nema_device_name(char* out, int len);

NEMA_IMPORT("nema", "log")
extern void nema_log(const char* level, const char* tag, const char* msg);

int main(int argc, char* argv[]) {
    char device[64] = {0};
    nema_device_name(device, sizeof(device));

    const char* who = (argc > 1) ? argv[1] : "World";
    printf("Hello, %s! Running on %s.\n", who, device);
    nema_log("info", "hello-wasm", "hello sent");
    return 0;
}
