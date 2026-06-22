// counter-wasm — Plan 86 Example (G3/G4)
// Storage-backed counter with both CLI and retained-UI modes.
//
// CLI (headless):
//   run counter-wasm inc    → increment and print
//   run counter-wasm dec    → decrement and print
//   run counter-wasm reset  → reset to 0 and print
//   run counter-wasm get    → print current value
//
// UI retained (G4):
//   run counter-wasm        → open UI window with Inc/Dec/Reset buttons
//   run counter-wasm --ui   → same, explicit flag
#include "nema_api.h"

static int readCount(void) {
    char buf[16] = "0";
    nema_storage_fs_read_file("count.txt", buf, sizeof(buf));
    return nema_atoi(buf);
}

static void writeCount(int n) {
    char buf[16];
    nema_itoa(n, buf);
    nema_storage_fs_write_file("count.txt", buf, nema_strlen(buf));
}

NEMA_EXPORT int main(void) {
    int count = readCount();

    char sub[32] = "";
    if (nema_argc() > 1) nema_argv_get(1, sub, sizeof(sub));

    // CLI subcommands
    if (sub[0] && nema_strcmp(sub, "--ui") != 0) {
        if (nema_strcmp(sub, "inc") == 0)        count++;
        else if (nema_strcmp(sub, "dec") == 0)   count--;
        else if (nema_strcmp(sub, "reset") == 0) count = 0;
        else if (nema_strcmp(sub, "get") != 0) {
            printf("usage: counter-wasm [inc|dec|reset|get|--ui]\n");
            return 1;
        }
        writeCount(count);
        printf("count: %d\n", count);
        return 0;
    }

    // Retained UI (no args or --ui)
    while (1) {
        ui_begin();
        ui_title("Counter");
        char label[24];
        // "Count: N"
        int i = 0;
        const char* p = "Count: ";
        while (*p) label[i++] = *p++;
        char num[12];
        nema_itoa(count, num);
        int j = 0;
        while (num[j]) label[i++] = num[j++];
        label[i] = '\0';
        ui_text(label);
        ui_button("+1",    1);
        ui_button("-1",    2);
        ui_button("Reset", 3);
        ui_end();

        int ev = ui_wait_event();
        if (ev == EV_BACK) break;
        if (ev == 1)      count++;
        else if (ev == 2) count--;
        else if (ev == 3) count = 0;
        writeCount(count);
    }
    return 0;
}
