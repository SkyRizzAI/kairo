// hello-wasm — Plan 86 Example (G1/G2/G3/G6)
// CLI-only app (no UI calls) — runs as terminal screen.
// Usage: run hello-wasm [name]
//   no args  → "Hello, World!"
//   run hello-wasm Budi → "Hello, Budi!"
#include "nema_api.h"

NEMA_EXPORT int main(void) {
    char name[64] = "World";
    if (nema_argc() > 1) {
        nema_argv_get(1, name, sizeof(name));
    }

    char device[64] = "";
    nema_device_name(device, sizeof(device));

    printf("Hello, %s!\n", name);
    printf("Running on %s\n", device);
    return 0;
}
