// Counter — WASM bare-metal (Plan 85)
// Storage-backed counter. CLI args: inc | dec | reset (default: print current).
// No stdio.h / libc — all I/O via nema_api.h host imports.
//
// Build: bun run app:build:counter-wasm

#include "nema_api.h"

#define STORAGE_FILE "count.txt"
#define BUF_SIZE 16

static int read_count(void) {
    char buf[BUF_SIZE];
    nema_memset(buf, 0, BUF_SIZE);
    int n = nema_storage_fs_read_file(STORAGE_FILE, buf, BUF_SIZE - 1);
    if (n <= 0) return 0;
    return nema_atoi(buf);
}

static void write_count(int n) {
    char buf[BUF_SIZE];
    nema_itoa(n, buf);
    nema_storage_fs_write_file(STORAGE_FILE, buf, nema_strlen(buf));
}

static int streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

NEMA_EXPORT int main(int argc, char* argv[]) {
    int n = read_count();

    if (argc > 1) {
        if      (streq(argv[1], "inc"))   n++;
        else if (streq(argv[1], "dec"))   n--;
        else if (streq(argv[1], "reset")) n = 0;
        write_count(n);
        nema_log("info", "counter-wasm", "count updated");
    }

    char num[12];
    nema_itoa(n, num);
    char msg[32] = "Count: ";
    int i = 7, j = 0;
    while (num[j]) msg[i++] = num[j++];
    msg[i] = '\0';
    nema_print(msg);
    return 0;
}
