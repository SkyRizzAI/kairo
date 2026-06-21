// Counter — WASM/C version (Plan 84 Fase 4).
// CLI mode: same logic as examples/counter/App.tsx tapi di C, jalan headless.
//
// Build (setelah Plan 84 Fase 4 selesai):
//   wasi-sdk: clang --target=wasm32-wasi -o counter.wasm main.c
//   Emscripten: emcc -o counter.wasm main.c -s STANDALONE_WASM
//
// Usage dari CLI:
//   counter-wasm         → print "Count: N"
//   counter-wasm inc     → increment, print "Count: N+1"
//   counter-wasm dec     → decrement, print "Count: N-1"
//   counter-wasm reset   → reset ke 0, print "Count: 0"
//
// nema.* host imports disediakan oleh firmware saat WASM diload.
// Lihat: Plan 84 Fase 4 — WASM nema API bridge.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── nema host imports (akan disediakan oleh WasmEngine) ──────────────────────
// Deklarasi ini match dengan import table yang di-emit IDL generator.

#ifdef __wasm__
#  define NEMA_IMPORT(mod, name) __attribute__((import_module(mod), import_name(name)))
#else
#  define NEMA_IMPORT(mod, name)  // host build: no-op, for IDE/lint only
#endif

NEMA_IMPORT("nema", "storage_fs_read_file")
extern int nema_storage_fs_read_file(const char* name, char* out_buf, int buf_len);

NEMA_IMPORT("nema", "storage_fs_write_file")
extern int nema_storage_fs_write_file(const char* name, const char* data, int data_len);

NEMA_IMPORT("nema", "log")
extern void nema_log(const char* level, const char* tag, const char* msg);

// ── counter logic ─────────────────────────────────────────────────────────────

#define STORAGE_FILE "count.txt"
#define BUF_SIZE 32

static int read_count(void) {
    char buf[BUF_SIZE] = {0};
    int n = nema_storage_fs_read_file(STORAGE_FILE, buf, sizeof(buf) - 1);
    if (n <= 0) return 0;
    return atoi(buf);
}

static void write_count(int n) {
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "%d", n);
    nema_storage_fs_write_file(STORAGE_FILE, buf, (int)strlen(buf));
}

int main(int argc, char* argv[]) {
    int n = read_count();

    if (argc > 1) {
        if (strcmp(argv[1], "inc") == 0)   n++;
        else if (strcmp(argv[1], "dec") == 0)   n--;
        else if (strcmp(argv[1], "reset") == 0) n = 0;
        write_count(n);
        nema_log("info", "counter-wasm", "count updated");
    }

    printf("Count: %d\n", n);
    return 0;
}
