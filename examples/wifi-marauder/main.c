// WiFi Marauder — Palanu WASM App
//
// Native Palanu WiFi audit tool. Runs directly on the ESP32 radio —
// no external board, no UART round-trip.
//
// UI: retained-mode widget system (Plan 86 Fase 3).
//
// Menu structure:
//   Main Menu
//     Scan APs → AP List → AP Detail → Deauth Flood
//                                     → Probe Flood (targeted)
//                                     → Signal Monitor
//     Sniff    → Beacons  (passive AP discovery)
//              → Probes   (what devices search for)
//              → Deauths  (detect deauth attacks)
//              → PMKID    (WPA2 handshake capture)
//              → Raw      (802.11 frame-type counters)
//     Beacon Spam (custom SSIDs from file or defaults)
//     Rickroll    (beacon spam w/ rickroll SSIDs)
//     About
//
// Build:
//   cd examples/wifi-marauder
//   clang --target=wasm32-unknown-unknown -nostdlib -O2 \
//         -I../../packages/app-sdk/include \
//         -Wl,--no-entry -Wl,--export=main \
//         -Wl,--allow-undefined -Wl,--strip-all \
//         -o dist/wifi-marauder.wasm main.c

#include "nema_api.h"

// ── Constants ──────────────────────────────────────────────────────────────────

#define MAX_APS           20
#define MAX_SSID_LEN      33
#define MAX_BSSID_LEN     18
#define SCAN_BUF_SZ       2048
#define FRAME_BUF_SZ      2500
#define EVENT_BUF_SZ      256
#define MAX_SNIFF_APS     16
#define MAX_SNIFF_PROBES  16
#define MAX_SNIFF_DEAUTHS 16
#define MAX_EAPOL_CAPS    8

// Default beacon spam SSIDs
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

// Rickroll SSIDs
#define RICKROLL_COUNT 8
static const char kRickrollSsids[] =
    "Never Gonna Give You Up\0"
    "Never Gonna Let You Down\0"
    "Never Gonna Run Around\0"
    "And Desert You\0"
    "Never Gonna Make You Cry\0"
    "Never Gonna Say Goodbye\0"
    "Never Gonna Tell a Lie\0"
    "And Hurt You\0";

// ── Types ──────────────────────────────────────────────────────────────────────

typedef struct {
    char bssid[MAX_BSSID_LEN];
    char ssid[MAX_SSID_LEN];
    int  channel;
    int  rssi;
    char auth[16];
} Ap;

typedef struct {
    char bssid[MAX_BSSID_LEN];
    char ssid[MAX_SSID_LEN];
    int  channel;
    int  count;
} SniffAp;

typedef struct {
    char ssid[MAX_SSID_LEN];
    char src[MAX_BSSID_LEN];
    int  count;
} SniffProbe;

typedef struct {
    char bssid[MAX_BSSID_LEN];
    char sta[MAX_BSSID_LEN];
    int  reason;
    int  count;
} SniffDeauth;

typedef struct {
    char          bssid[MAX_BSSID_LEN];
    char          sta[MAX_BSSID_LEN];
    int           msg;
    unsigned char pmkid[16];
    int           has_pmkid;
} EapolCap;

typedef struct {
    int total;
    int beacon;
    int probe;
    int deauth;
    int data;
    int other;
} FrameStats;

typedef enum {
    SCR_MAIN = 0,
    SCR_SCANNING,
    SCR_AP_LIST,
    SCR_AP_DETAIL,
    SCR_DEAUTHING,
    SCR_MONITORING,
    SCR_BEACON_SPAM,
    SCR_ABOUT,
    SCR_SNIFF_MENU,
    SCR_SNIFF_BEACONS,
    SCR_SNIFF_PROBES,
    SCR_SNIFF_DEAUTHS,
    SCR_SNIFF_PMKID,
    SCR_PROBE_FLOOD,
    SCR_RICKROLLING,
    SCR_SIGNAL_MON,
} Screen;

// ── Globals ────────────────────────────────────────────────────────────────────

static Ap      g_aps[MAX_APS];
static int     g_ap_count = 0;
static int     g_selected = 0;
static Screen  g_screen   = SCR_MAIN;
static int     g_mon_ch   = 1;
static int     g_sniff_ch = 1;

static SniffAp     g_sniff_aps[MAX_SNIFF_APS];       static int g_sniff_ap_count  = 0;
static SniffProbe  g_sniff_prb[MAX_SNIFF_PROBES];    static int g_sniff_prb_count = 0;
static SniffDeauth g_sniff_dau[MAX_SNIFF_DEAUTHS];   static int g_sniff_dau_count = 0;
static EapolCap    g_eapol[MAX_EAPOL_CAPS];           static int g_eapol_count     = 0;

// ── String utilities ───────────────────────────────────────────────────────────

static int str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int str_len(const char* s) { int n = 0; while (s[n]) n++; return n; }

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void str_append(char* dst, int cap, int* used, const char* src) {
    while (*src && *used < cap - 1) dst[(*used)++] = *src++;
    dst[*used] = '\0';
}

static const char* parse_int(const char* s, int* out) {
    int neg = 0, v = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    *out = neg ? -v : v;
    return s;
}

// ── MAC / 802.11 frame utilities ──────────────────────────────────────────────

static void hex_mac(const unsigned char* b, char* out) {
    static const char h[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        out[i*3+0] = h[(b[i] >> 4) & 0xF];
        out[i*3+1] = h[b[i] & 0xF];
        out[i*3+2] = (i < 5) ? ':' : '\0';
    }
}

static void frame_bssid(const unsigned char* f, int n, char* out) {
    out[0] = '\0'; if (n < 22) return; hex_mac(f + 16, out);
}
static void frame_sa(const unsigned char* f, int n, char* out) {
    out[0] = '\0'; if (n < 16) return; hex_mac(f + 10, out);
}
static void frame_da(const unsigned char* f, int n, char* out) {
    out[0] = '\0'; if (n < 10) return; hex_mac(f + 4, out);
}

// Extract SSID IE (tag=0x00) from IEs starting at ie_start.
static void extract_ssid_ie(const unsigned char* f, int n, int ie_start, char* ssid_out) {
    ssid_out[0] = '\0';
    int ie = ie_start;
    while (ie + 2 <= n && ie < ie_start + 128) {
        unsigned char tag = f[ie], len = f[ie + 1];
        if (tag == 0x00) {
            if (len == 0) { str_copy(ssid_out, "(hidden)", 33); return; }
            if (ie + 2 + len > n || len > 32) return;
            for (int i = 0; i < (int)len; i++) ssid_out[i] = (char)f[ie + 2 + i];
            ssid_out[len] = '\0';
            return;
        }
        if (len == 0) break;  // avoid infinite loop on malformed IE
        ie += 2 + len;
    }
}

// Channel from DS Parameter Set IE (tag=0x03) in beacon body IEs at byte 36.
static int beacon_channel_ie(const unsigned char* f, int n) {
    if (n < 38) return 0;
    int ie = 36;
    while (ie + 2 <= n && ie < 36 + 256) {
        unsigned char tag = f[ie], len = f[ie + 1];
        if (tag == 0x03 && len == 1 && ie + 3 <= n) return f[ie + 2];
        if (len == 0) break;
        ie += 2 + len;
    }
    return 0;
}

// Deauth/disassoc reason code (body bytes 0-1, little-endian).
static int mgmt_reason(const unsigned char* f, int n) {
    if (n < 26) return 0;
    return (int)f[24] | ((int)f[25] << 8);
}

// Scan for EAPOL ethertype 0x888E in data frame (bytes 24..60).
// Returns start of EAPOL body (past ethertype), or -1.
static int find_eapol_offset(const unsigned char* f, int n) {
    for (int i = 24; i < n - 1 && i < 60; i++) {
        if (f[i] == 0x88 && f[i+1] == 0x8E) return i + 2;
    }
    return -1;
}

// EAPOL-Key WPA2 4-way handshake message number (1-4), or 0.
static int eapol_key_msg(const unsigned char* eapol, int n) {
    if (n < 7) return 0;
    if (eapol[1] != 3) return 0;   // not EAPOL-Key
    if (eapol[4] != 0x02) return 0; // not WPA2 descriptor
    unsigned char hi = eapol[5], lo = eapol[6];
    int pairwise = (lo >> 3) & 1;
    int install  = (lo >> 6) & 1;
    int ack      = (lo >> 7) & 1;
    int mic      = (hi >> 0) & 1;
    if (!pairwise) return 0;
    if (ack && !mic) return 1;
    if (!ack && mic && !install) {
        if (n >= 99) {
            unsigned int kdlen = ((unsigned int)eapol[97] << 8) | eapol[98];
            if (kdlen == 0) return 4;
        }
        return 2;
    }
    if (ack && mic && install) return 3;
    return 0;
}

// Try to extract PMKID from EAPOL-Key msg 1 key data KDE (OUI=00:0F:AC type=4).
static int try_extract_pmkid(const unsigned char* eapol, int n, unsigned char pmkid_out[16]) {
    if (n < 99) return 0;
    unsigned int kdlen = ((unsigned int)eapol[97] << 8) | eapol[98];
    if (kdlen < 22 || (int)(99 + kdlen) > n) return 0;
    const unsigned char* kd = eapol + 99;
    for (int i = 0; i + 22 <= (int)kdlen; i++) {
        if (kd[i]   == 0xDD && kd[i+1] == 0x14 &&
            kd[i+2] == 0x00 && kd[i+3] == 0x0F &&
            kd[i+4] == 0xAC && kd[i+5] == 0x04) {
            nema_memcpy(pmkid_out, kd + i + 6, 16);
            return 1;
        }
    }
    return 0;
}

// ── Sniff dedup helpers ────────────────────────────────────────────────────────

static void sniff_ap_seen(const char* bssid, const char* ssid, int ch) {
    for (int i = 0; i < g_sniff_ap_count; i++) {
        if (str_eq(g_sniff_aps[i].bssid, bssid)) { g_sniff_aps[i].count++; return; }
    }
    if (g_sniff_ap_count >= MAX_SNIFF_APS) return;
    int i = g_sniff_ap_count++;
    str_copy(g_sniff_aps[i].bssid, bssid, MAX_BSSID_LEN);
    str_copy(g_sniff_aps[i].ssid,  ssid,  MAX_SSID_LEN);
    g_sniff_aps[i].channel = ch; g_sniff_aps[i].count = 1;
}

static void sniff_probe_seen(const char* ssid, const char* src) {
    for (int i = 0; i < g_sniff_prb_count; i++) {
        if (str_eq(g_sniff_prb[i].ssid, ssid) && str_eq(g_sniff_prb[i].src, src))
            { g_sniff_prb[i].count++; return; }
    }
    if (g_sniff_prb_count >= MAX_SNIFF_PROBES) return;
    int i = g_sniff_prb_count++;
    str_copy(g_sniff_prb[i].ssid, ssid, MAX_SSID_LEN);
    str_copy(g_sniff_prb[i].src,  src,  MAX_BSSID_LEN);
    g_sniff_prb[i].count = 1;
}

static void sniff_deauth_seen(const char* bssid, const char* sta, int reason) {
    for (int i = 0; i < g_sniff_dau_count; i++) {
        if (str_eq(g_sniff_dau[i].bssid, bssid) && str_eq(g_sniff_dau[i].sta, sta))
            { g_sniff_dau[i].count++; return; }
    }
    if (g_sniff_dau_count >= MAX_SNIFF_DEAUTHS) return;
    int i = g_sniff_dau_count++;
    str_copy(g_sniff_dau[i].bssid, bssid, MAX_BSSID_LEN);
    str_copy(g_sniff_dau[i].sta,   sta,   MAX_BSSID_LEN);
    g_sniff_dau[i].reason = reason; g_sniff_dau[i].count = 1;
}

static void eapol_seen(const char* bssid, const char* sta, int msg,
                        const unsigned char* pmkid, int has_pmkid) {
    for (int i = 0; i < g_eapol_count; i++) {
        if (str_eq(g_eapol[i].bssid, bssid) && str_eq(g_eapol[i].sta, sta)) {
            if (msg > g_eapol[i].msg) g_eapol[i].msg = msg;
            if (has_pmkid && !g_eapol[i].has_pmkid)
                { nema_memcpy(g_eapol[i].pmkid, pmkid, 16); g_eapol[i].has_pmkid = 1; }
            return;
        }
    }
    if (g_eapol_count >= MAX_EAPOL_CAPS) return;
    int i = g_eapol_count++;
    str_copy(g_eapol[i].bssid, bssid, MAX_BSSID_LEN);
    str_copy(g_eapol[i].sta,   sta,   MAX_BSSID_LEN);
    g_eapol[i].msg = msg; g_eapol[i].has_pmkid = has_pmkid;
    if (has_pmkid) nema_memcpy(g_eapol[i].pmkid, pmkid, 16);
}

// ── Scan result parser ─────────────────────────────────────────────────────────

static int parse_scan_results(const char* buf, int len) {
    int count = 0;
    const char* p = buf;
    const char* end = buf + len;
    while (p < end && count < MAX_APS) {
        Ap* ap = &g_aps[count];
        int i = 0;
        while (p < end && *p != '|' && i < MAX_BSSID_LEN - 1) ap->bssid[i++] = *p++;
        ap->bssid[i] = '\0'; if (p < end) p++;
        i = 0;
        while (p < end && *p != '|' && i < MAX_SSID_LEN - 1) ap->ssid[i++] = *p++;
        ap->ssid[i] = '\0'; if (p < end) p++;
        p = parse_int(p, &ap->channel); if (p < end && *p == '|') p++;
        p = parse_int(p, &ap->rssi);    if (p < end && *p == '|') p++;
        i = 0;
        while (p < end && *p != '\n' && i < 15) ap->auth[i++] = *p++;
        ap->auth[i] = '\0';
        if (p < end && *p == '\n') p++;
        if (ap->bssid[0]) count++;
    }
    return count;
}

// Find RSSI for a specific BSSID in scan result buffer. Returns -127 if missing.
static int find_rssi_for_bssid(const char* buf, int len, const char* target) {
    const char* p = buf;
    const char* end = buf + len;
    while (p < end) {
        char bssid[MAX_BSSID_LEN] = {};
        int i = 0;
        while (p < end && *p != '|' && i < MAX_BSSID_LEN - 1) bssid[i++] = *p++;
        bssid[i] = '\0'; if (p < end) p++;
        while (p < end && *p != '|') p++; if (p < end) p++;  // skip SSID
        while (p < end && *p != '|') p++; if (p < end) p++;  // skip channel
        int rssi = 0; p = parse_int(p, &rssi);
        while (p < end && *p != '\n') p++; if (p < end) p++;
        if (str_eq(bssid, target)) return rssi;
    }
    return -127;
}

// ── 802.11 frame type classifier ──────────────────────────────────────────────

static void classify_frame(const unsigned char* f, int n, FrameStats* s) {
    if (n < 2) { s->other++; return; }
    int type    = (f[0] >> 2) & 0x3;
    int subtype = (f[0] >> 4) & 0xF;
    s->total++;
    if (type == 0) {
        if (subtype == 8)                      s->beacon++;
        else if (subtype == 4 || subtype == 5) s->probe++;
        else if (subtype == 12)                s->deauth++;
        else                                   s->other++;
    } else if (type == 2) {
        s->data++;
    } else {
        s->other++;
    }
}

// ── UI helpers ─────────────────────────────────────────────────────────────────

static void ap_label(const Ap* ap, char* buf, int cap) {
    int used = 0;
    str_append(buf, cap, &used, ap->ssid[0] ? ap->ssid : "(hidden)");
    str_append(buf, cap, &used, "  CH");
    char tmp[8]; char* tp = tmp + 7; *tp = '\0';
    int ch = ap->channel;
    if (ch == 0) { *--tp = '0'; } else { while (ch) { *--tp = '0' + ch % 10; ch /= 10; } }
    str_append(buf, cap, &used, tp);
    str_append(buf, cap, &used, " ");
    int rs = ap->rssi;
    if (rs < 0) { str_append(buf, cap, &used, "-"); rs = -rs; }
    tp = tmp + 7; *tp = '\0';
    if (rs == 0) { *--tp = '0'; } else { while (rs) { *--tp = '0' + rs % 10; rs /= 10; } }
    str_append(buf, cap, &used, tp);
    str_append(buf, cap, &used, "dBm");
}

static const char* itoa_s(int n) {
    static char buf[16];
    char* tp = buf + 15; *tp = '\0';
    int neg = (n < 0); if (neg) n = -n;
    if (n == 0) { *--tp = '0'; } else { while (n) { *--tp = '0' + n % 10; n /= 10; } }
    if (neg) *--tp = '-';
    return tp;
}

// Signal bar: -100..-30 dBm → 0..10 blocks
static void rssi_bar(int rssi, char* out, int cap) {
    int bars = (rssi + 100) * 10 / 70;
    if (bars < 0) bars = 0; if (bars > 10) bars = 10;
    int used = 0; out[0] = '\0';
    str_append(out, cap, &used, "[");
    for (int i = 0; i < 10; i++) str_append(out, cap, &used, i < bars ? "#" : ".");
    str_append(out, cap, &used, bars >= 8 ? "] Exc" :
                                bars >= 6 ? "] Good" :
                                bars >= 4 ? "] Fair" :
                                bars >= 2 ? "] Weak" : "] None");
}

// Shared CH+/CH- handler for sniff screens. Returns 1 if channel changed.
static int sniff_handle_ch(int ev, int* open_flag) {
    if (ev == 1 && g_sniff_ch < 13) {
        g_sniff_ch++;
        if (*open_flag) { wifi_monitor_close(); *open_flag = (wifi_monitor_open(g_sniff_ch) == 0); }
        return 1;
    }
    if (ev == 2 && g_sniff_ch > 1) {
        g_sniff_ch--;
        if (*open_flag) { wifi_monitor_close(); *open_flag = (wifi_monitor_open(g_sniff_ch) == 0); }
        return 1;
    }
    return 0;
}

// ── Main Menu ─────────────────────────────────────────────────────────────────
#define MMENU_SCAN   1
#define MMENU_SNIFF  2
#define MMENU_BEACON 3
#define MMENU_RICK   4
#define MMENU_ABOUT  5

static int screen_main(void) {
    ui_begin();
    ui_title("WiFi Marauder");
    ui_button("Scan APs",    MMENU_SCAN);
    ui_button("Sniff",       MMENU_SNIFF);
    ui_button("Beacon Spam", MMENU_BEACON);
    ui_button("Rickroll",    MMENU_RICK);
    ui_button("About",       MMENU_ABOUT);
    ui_end();
    return ui_wait_event();
}

// ── Scanning ──────────────────────────────────────────────────────────────────

static int screen_scanning(void) {
    ui_begin();
    ui_title("Scanning...");
    ui_text("Searching for APs");
    ui_text("Please wait...");
    ui_end();

    static char scan_buf[SCAN_BUF_SZ];
    int n = wifi_scan(scan_buf, sizeof(scan_buf));
    g_ap_count = (n > 0) ? parse_scan_results(scan_buf, n) : 0;
    g_selected = 0;
    g_screen = SCR_AP_LIST;
    return 0;
}

// ── AP List ───────────────────────────────────────────────────────────────────

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
        ui_button(label, i + 1);
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
#define BTN_PROBE   2
#define BTN_SIGNAL  3
#define BTN_BACK_D  4

static int screen_ap_detail(void) {
    const Ap* ap = &g_aps[g_selected];
    ui_begin();
    ui_title("Target AP");
    ui_text(ap->ssid[0] ? ap->ssid : "(hidden)");
    ui_text(ap->bssid);
    {
        char line[40] = "CH: ";
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
    ui_button("Deauth Flood",   BTN_DEAUTH);
    ui_button("Probe Flood",    BTN_PROBE);
    ui_button("Signal Monitor", BTN_SIGNAL);
    ui_button("< Back",         BTN_BACK_D);
    ui_end();

    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == BTN_BACK_D) g_screen = SCR_AP_LIST;
    else if (ev == BTN_DEAUTH)             g_screen = SCR_DEAUTHING;
    else if (ev == BTN_PROBE)              g_screen = SCR_PROBE_FLOOD;
    else if (ev == BTN_SIGNAL)             g_screen = SCR_SIGNAL_MON;
    return ev;
}

// ── Deauth Flood ──────────────────────────────────────────────────────────────

static void screen_deauthing(void) {
    const Ap* ap = &g_aps[g_selected];
    int started = (wifi_deauth_start(ap->bssid, ap->channel) == 0);
    int packets = 0;
    char ev_buf[EVENT_BUF_SZ];

    while (1) {
        int n = wifi_wait_event(ev_buf, sizeof(ev_buf), 100);
        if (n > 0) packets++;

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

// ── Probe Flood ───────────────────────────────────────────────────────────────
// Targeted probe flood: uses the AP's SSID + channel.
// Channel can be changed live with CH+/CH-.

static void screen_probe_flood(void) {
    const Ap* ap = &g_aps[g_selected];
    int ch = ap->channel > 0 ? ap->channel : 1;
    int started = (wifi_probe_flood_start(ap->ssid, ch) == 0);
    int ticks = 0;
    char ev_buf[EVENT_BUF_SZ];

    while (1) {
        int n = wifi_wait_event(ev_buf, sizeof(ev_buf), 100);
        if (n > 0) ticks++;

        ui_begin();
        {
            char hdr[24] = "Probe Flood  CH";
            int used = 15;
            str_append(hdr, sizeof(hdr), &used, itoa_s(ch));
            ui_title(hdr);
        }
        if (!started) {
            ui_text("Not supported");
        } else {
            char line[40] = "SSID: ";
            int used = 6;
            str_append(line, sizeof(line), &used, ap->ssid[0] ? ap->ssid : "(wildcard)");
            ui_text(line);
            char cnt[32] = "Probes sent: ~";
            used = 14;
            str_append(cnt, sizeof(cnt), &used, itoa_s(ticks * 20));
            ui_text(cnt);
        }
        ui_row_begin();
        ui_button("CH+", 1);
        ui_button("CH-", 2);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int key = ui_poll_event();
        if (key == EV_BACK || key == 3) break;
        if (key == 1 && ch < 13) {
            ch++;
            if (started) { wifi_probe_flood_stop(); started = (wifi_probe_flood_start(ap->ssid, ch) == 0); }
        } else if (key == 2 && ch > 1) {
            ch--;
            if (started) { wifi_probe_flood_stop(); started = (wifi_probe_flood_start(ap->ssid, ch) == 0); }
        }
    }

    wifi_probe_flood_stop();
    g_screen = SCR_AP_DETAIL;
}

// ── Signal Monitor ─────────────────────────────────────────────────────────────

static void screen_signal_mon(void) {
    const Ap* ap = &g_aps[g_selected];
    int rssi_min  =  0;
    int rssi_max  = -127;
    int samples   =  0;
    int current   = -127;
    static char scan_buf[SCAN_BUF_SZ];
    char bar[24];

    while (1) {
        ui_begin();
        {
            char hdr[48] = "Signal: ";
            int used = 8;
            str_append(hdr, sizeof(hdr), &used, ap->ssid[0] ? ap->ssid : ap->bssid);
            ui_title(hdr);
        }
        if (current == -127 && samples == 0) {
            ui_text("Scanning...");
        } else if (current == -127) {
            ui_text("AP not found");
        } else {
            char line[40];
            int used;
            used = 0; line[0] = '\0';
            str_append(line, sizeof(line), &used, "RSSI: ");
            str_append(line, sizeof(line), &used, itoa_s(current));
            str_append(line, sizeof(line), &used, " dBm");
            ui_text(line);
            rssi_bar(current, bar, sizeof(bar));
            ui_text(bar);
            used = 0; line[0] = '\0';
            str_append(line, sizeof(line), &used, "Min:");
            str_append(line, sizeof(line), &used, itoa_s(rssi_min));
            str_append(line, sizeof(line), &used, " Max:");
            str_append(line, sizeof(line), &used, itoa_s(rssi_max));
            ui_text(line);
            used = 0; line[0] = '\0';
            str_append(line, sizeof(line), &used, "Samples: ");
            str_append(line, sizeof(line), &used, itoa_s(samples));
            ui_text(line);
        }
        ui_button("Stop", 1);
        ui_end();

        int key = ui_poll_event();
        if (key == EV_BACK || key == 1) break;

        // Blocking scan (~2s)
        int n = wifi_scan(scan_buf, sizeof(scan_buf));
        if (n > 0) {
            int r = find_rssi_for_bssid(scan_buf, n, ap->bssid);
            if (r != -127) {
                current = r;
                samples++;
                if (samples == 1 || r < rssi_min) rssi_min = r;
                if (samples == 1 || r > rssi_max) rssi_max = r;
            }
        }
    }

    g_screen = SCR_AP_DETAIL;
}

// ── Monitor Mode (Raw frame stats) ────────────────────────────────────────────

static void screen_monitoring(void) {
    g_mon_ch = 1;
    int open = (wifi_monitor_open(g_mon_ch) == 0);
    FrameStats stats;
    stats.total=0; stats.beacon=0; stats.probe=0;
    stats.deauth=0; stats.data=0; stats.other=0;
    static unsigned char frame_buf[FRAME_BUF_SZ];

    while (1) {
        if (open) {
            int n = wifi_monitor_read(frame_buf, sizeof(frame_buf), 50);
            if (n > 0) classify_frame(frame_buf, n, &stats);
        }

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
    used = 0; line[0] = '\0'; \
    str_append(line, sizeof(line), &used, lbl); \
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
        ui_button("CH+", 1);
        ui_button("CH-", 2);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        if (ev == 1 && g_mon_ch < 13) {
            g_mon_ch++;
            if (open) { wifi_monitor_close(); open = (wifi_monitor_open(g_mon_ch) == 0); }
        } else if (ev == 2 && g_mon_ch > 1) {
            g_mon_ch--;
            if (open) { wifi_monitor_close(); open = (wifi_monitor_open(g_mon_ch) == 0); }
        }
    }

    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Sniff Menu ────────────────────────────────────────────────────────────────

static int screen_sniff_menu(void) {
    ui_begin();
    ui_title("Sniff");
    ui_button("Beacons",     1);
    ui_button("Probes",      2);
    ui_button("Deauths",     3);
    ui_button("PMKID",       4);
    ui_button("Raw Monitor", 5);
    ui_button("< Back",      99);
    ui_end();

    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 99) g_screen = SCR_MAIN;
    else if (ev == 1) g_screen = SCR_SNIFF_BEACONS;
    else if (ev == 2) g_screen = SCR_SNIFF_PROBES;
    else if (ev == 3) g_screen = SCR_SNIFF_DEAUTHS;
    else if (ev == 4) g_screen = SCR_SNIFF_PMKID;
    else if (ev == 5) g_screen = SCR_MONITORING;
    return ev;
}

// ── Sniff Beacons ─────────────────────────────────────────────────────────────

static void screen_sniff_beacons(void) {
    g_sniff_ch = 1;
    g_sniff_ap_count = 0;
    int open = (wifi_monitor_open(g_sniff_ch) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];

    while (1) {
        int n = wifi_monitor_read(fbuf, sizeof(fbuf), 50);
        if (n >= 38) {
            if ((fbuf[0] >> 2 & 0x3) == 0 && (fbuf[0] >> 4 & 0xF) == 8) {
                char bssid[MAX_BSSID_LEN], ssid[MAX_SSID_LEN];
                frame_bssid(fbuf, n, bssid);
                extract_ssid_ie(fbuf, n, 36, ssid);
                int ch = beacon_channel_ie(fbuf, n);
                if (bssid[0]) sniff_ap_seen(bssid, ssid, ch);
            }
        }

        ui_begin();
        {
            char hdr[28] = "Beacons  CH";
            int used = 11;
            str_append(hdr, sizeof(hdr), &used, itoa_s(g_sniff_ch));
            ui_title(hdr);
        }
        {
            char cnt[32] = "Found: ";
            int used = 7;
            str_append(cnt, sizeof(cnt), &used, itoa_s(g_sniff_ap_count));
            ui_text(cnt);
        }
        for (int i = 0; i < g_sniff_ap_count; i++) {
            char line[48];
            int used = 0; line[0] = '\0';
            str_append(line, sizeof(line), &used, g_sniff_aps[i].ssid[0] ? g_sniff_aps[i].ssid : "(hidden)");
            if (g_sniff_aps[i].channel) {
                str_append(line, sizeof(line), &used, " CH");
                str_append(line, sizeof(line), &used, itoa_s(g_sniff_aps[i].channel));
            }
            ui_text(line);
        }
        if (g_sniff_ap_count == 0) ui_text("Listening...");
        ui_row_begin();
        ui_button("CH+", 1);
        ui_button("CH-", 2);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        sniff_handle_ch(ev, &open);
    }

    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Sniff Probes ──────────────────────────────────────────────────────────────

static void screen_sniff_probes(void) {
    g_sniff_ch = 1;
    g_sniff_prb_count = 0;
    int open = (wifi_monitor_open(g_sniff_ch) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];

    while (1) {
        int n = wifi_monitor_read(fbuf, sizeof(fbuf), 50);
        if (n >= 26) {
            int type = (fbuf[0] >> 2) & 0x3, subtype = (fbuf[0] >> 4) & 0xF;
            if (type == 0 && subtype == 4) {  // probe request
                char src[MAX_BSSID_LEN], ssid[MAX_SSID_LEN];
                frame_sa(fbuf, n, src);
                extract_ssid_ie(fbuf, n, 24, ssid);  // probe req: IEs at 24
                if (src[0]) sniff_probe_seen(ssid, src);
            }
        }

        ui_begin();
        {
            char hdr[28] = "Probes  CH";
            int used = 10;
            str_append(hdr, sizeof(hdr), &used, itoa_s(g_sniff_ch));
            ui_title(hdr);
        }
        {
            char cnt[32] = "Seen: ";
            int used = 6;
            str_append(cnt, sizeof(cnt), &used, itoa_s(g_sniff_prb_count));
            ui_text(cnt);
        }
        for (int i = 0; i < g_sniff_prb_count; i++) {
            char line[56];
            int used = 0; line[0] = '\0';
            const char* ssid = g_sniff_prb[i].ssid[0] ? g_sniff_prb[i].ssid : "(wildcard)";
            str_append(line, sizeof(line), &used, ssid);
            str_append(line, sizeof(line), &used, " x");
            str_append(line, sizeof(line), &used, itoa_s(g_sniff_prb[i].count));
            ui_text(line);
        }
        if (g_sniff_prb_count == 0) ui_text("Listening...");
        ui_row_begin();
        ui_button("CH+", 1);
        ui_button("CH-", 2);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        sniff_handle_ch(ev, &open);
    }

    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Sniff Deauths ─────────────────────────────────────────────────────────────

static void screen_sniff_deauths(void) {
    g_sniff_ch = 1;
    g_sniff_dau_count = 0;
    int open = (wifi_monitor_open(g_sniff_ch) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];

    while (1) {
        int n = wifi_monitor_read(fbuf, sizeof(fbuf), 50);
        if (n >= 26) {
            int type = (fbuf[0] >> 2) & 0x3, subtype = (fbuf[0] >> 4) & 0xF;
            if (type == 0 && (subtype == 12 || subtype == 10)) {  // deauth or disassoc
                char bssid[MAX_BSSID_LEN], da[MAX_BSSID_LEN];
                frame_bssid(fbuf, n, bssid);
                frame_da(fbuf, n, da);
                int reason = mgmt_reason(fbuf, n);
                if (bssid[0]) sniff_deauth_seen(bssid, da, reason);
            }
        }

        ui_begin();
        {
            char hdr[28] = "Deauths  CH";
            int used = 11;
            str_append(hdr, sizeof(hdr), &used, itoa_s(g_sniff_ch));
            ui_title(hdr);
        }
        {
            char cnt[32] = "Caught: ";
            int used = 8;
            str_append(cnt, sizeof(cnt), &used, itoa_s(g_sniff_dau_count));
            ui_text(cnt);
        }
        for (int i = 0; i < g_sniff_dau_count; i++) {
            char line[56];
            int used = 0; line[0] = '\0';
            // First 11 chars of BSSID + reason + count
            for (int c = 0; c < 11 && g_sniff_dau[i].bssid[c]; c++)
                line[used++] = g_sniff_dau[i].bssid[c];
            line[used] = '\0';
            str_append(line, sizeof(line), &used, " R");
            str_append(line, sizeof(line), &used, itoa_s(g_sniff_dau[i].reason));
            if (g_sniff_dau[i].count > 1) {
                str_append(line, sizeof(line), &used, " x");
                str_append(line, sizeof(line), &used, itoa_s(g_sniff_dau[i].count));
            }
            ui_text(line);
        }
        if (g_sniff_dau_count == 0) ui_text("Listening...");
        ui_row_begin();
        ui_button("CH+", 1);
        ui_button("CH-", 2);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        sniff_handle_ch(ev, &open);
    }

    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Sniff PMKID ───────────────────────────────────────────────────────────────
// Captures WPA2 EAPOL 4-way handshake. Message 1 may contain the PMKID KDE
// (present when the AP supports PMKID-less auth — most modern WPA2 APs).

static void screen_sniff_pmkid(void) {
    g_sniff_ch = 1;
    g_eapol_count = 0;
    int open = (wifi_monitor_open(g_sniff_ch) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];

    while (1) {
        int n = wifi_monitor_read(fbuf, sizeof(fbuf), 50);
        if (n >= 28) {
            int type = (fbuf[0] >> 2) & 0x3;
            if (type == 2) {  // data frame
                int eapol_off = find_eapol_offset(fbuf, n);
                if (eapol_off >= 0 && eapol_off + 7 <= n) {
                    const unsigned char* ep = fbuf + eapol_off;
                    int rem = n - eapol_off;
                    int msg = eapol_key_msg(ep, rem);
                    if (msg > 0) {
                        char bssid[MAX_BSSID_LEN], sta[MAX_BSSID_LEN];
                        frame_bssid(fbuf, n, bssid);
                        frame_sa(fbuf, n, sta);
                        unsigned char pmkid[16];
                        int has = (msg == 1) ? try_extract_pmkid(ep, rem, pmkid) : 0;
                        if (bssid[0]) eapol_seen(bssid, sta, msg, pmkid, has);
                    }
                }
            }
        }

        ui_begin();
        {
            char hdr[28] = "PMKID  CH";
            int used = 9;
            str_append(hdr, sizeof(hdr), &used, itoa_s(g_sniff_ch));
            ui_title(hdr);
        }
        {
            char cnt[32] = "Handshakes: ";
            int used = 12;
            str_append(cnt, sizeof(cnt), &used, itoa_s(g_eapol_count));
            ui_text(cnt);
        }
        for (int i = 0; i < g_eapol_count; i++) {
            char line[56];
            int used = 0; line[0] = '\0';
            // Truncated BSSID + msg # + PMKID indicator
            for (int c = 0; c < 11 && g_eapol[i].bssid[c]; c++)
                line[used++] = g_eapol[i].bssid[c];
            line[used] = '\0';
            str_append(line, sizeof(line), &used, " M");
            str_append(line, sizeof(line), &used, itoa_s(g_eapol[i].msg));
            if (g_eapol[i].has_pmkid) str_append(line, sizeof(line), &used, " *PMKID*");
            ui_text(line);
        }
        if (g_eapol_count == 0) ui_text("Waiting for handshake...");
        ui_row_begin();
        ui_button("CH+", 1);
        ui_button("CH-", 2);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        sniff_handle_ch(ev, &open);
    }

    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Beacon Spam ───────────────────────────────────────────────────────────────

static void screen_beacon_spam(void) {
    static char custom_buf[512];
    int custom_count = 0;
    int n = nema_storage_fs_read_file("spam_ssids.txt", custom_buf, sizeof(custom_buf) - 1);
    if (n > 0) {
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

// ── Rickroll ──────────────────────────────────────────────────────────────────

static void screen_rickrolling(void) {
    int started = (wifi_beacon_spam_start(kRickrollSsids, RICKROLL_COUNT) == 0);
    int ticks = 0;
    char ev_buf[EVENT_BUF_SZ];

    while (1) {
        int ev_n = wifi_wait_event(ev_buf, sizeof(ev_buf), 200);
        if (ev_n > 0) ticks++;

        ui_begin();
        ui_title("Rickroll");
        if (!started) {
            ui_text("Not supported on this board");
        } else {
            ui_text("Never gonna give you up");
            ui_text("Never gonna let you down");
            char cnt[32] = "Ticks: ";
            int used = 7;
            str_append(cnt, sizeof(cnt), &used, itoa_s(ticks));
            ui_text(cnt);
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
    ui_text("v2.0.0 — Palanu WASM");
    ui_text("Native ESP32 radio");
    ui_text("No external board needed");
    ui_text("");
    ui_text("Features:");
    ui_text("  Scan + Targeted Attacks");
    ui_text("  Deauth + Probe Flood");
    ui_text("  Signal Monitor");
    ui_text("  Sniff: Beacons / Probes");
    ui_text("  Sniff: Deauths / PMKID");
    ui_text("  Beacon Spam + Rickroll");
    ui_text("");
    ui_text("Inspired by Flipper Zero");
    ui_text("Marauder by justcallmekoko");
    ui_button("Back", 1);
    ui_end();
    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 1) g_screen = SCR_MAIN;
    return ev;
}

// ── Entry point ────────────────────────────────────────────────────────────────

NEMA_EXPORT int main(void) {
    nema_log("info", "WiFiMarauder", "started v2.0");
    g_screen = SCR_MAIN;

    while (1) {
        switch (g_screen) {
            case SCR_MAIN: {
                int ev = screen_main();
                if (ev == EV_BACK)      return 0;
                if (ev == MMENU_SCAN)   g_screen = SCR_SCANNING;
                if (ev == MMENU_SNIFF)  g_screen = SCR_SNIFF_MENU;
                if (ev == MMENU_BEACON) g_screen = SCR_BEACON_SPAM;
                if (ev == MMENU_RICK)   g_screen = SCR_RICKROLLING;
                if (ev == MMENU_ABOUT)  g_screen = SCR_ABOUT;
                break;
            }
            case SCR_SCANNING:      screen_scanning();      break;
            case SCR_AP_LIST:       screen_ap_list();       break;
            case SCR_AP_DETAIL:     screen_ap_detail();     break;
            case SCR_DEAUTHING:     screen_deauthing();     break;
            case SCR_PROBE_FLOOD:   screen_probe_flood();   break;
            case SCR_SIGNAL_MON:    screen_signal_mon();    break;
            case SCR_MONITORING:    screen_monitoring();    break;
            case SCR_SNIFF_MENU:    screen_sniff_menu();    break;
            case SCR_SNIFF_BEACONS: screen_sniff_beacons(); break;
            case SCR_SNIFF_PROBES:  screen_sniff_probes();  break;
            case SCR_SNIFF_DEAUTHS: screen_sniff_deauths(); break;
            case SCR_SNIFF_PMKID:   screen_sniff_pmkid();   break;
            case SCR_BEACON_SPAM:   screen_beacon_spam();   break;
            case SCR_RICKROLLING:   screen_rickrolling();   break;
            case SCR_ABOUT:         screen_about();         break;
            default: g_screen = SCR_MAIN; break;
        }
    }
}
