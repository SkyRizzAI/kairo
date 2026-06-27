// Network Tools — Palanu WASM App
//
// LAN utilities that run over the device's NORMAL WiFi STA connection (no radio
// takeover): connection status, ARP host discovery, and a TCP port scan of the
// gateway. These were split out of WiFi Marauder (Plan 91) because they need a
// live STA link, whereas Marauder seizes the raw radio.
//
// Build: handled by `bun run app:build:network-tools` (clang → network.wasm).

#include "nema_api.h"

#define APP_VERSION "1.0.0"

// ── tiny libc-free helpers ─────────────────────────────────────────────────

static int str_len(const char* s) { int n = 0; while (s[n]) n++; return n; }

static void str_append(char* dst, int cap, int* used, const char* src) {
    while (*src && *used < cap - 1) dst[(*used)++] = *src++;
    dst[*used] = '\0';
}

static const char* itoa_s(int v) {
    static char buf[12];
    int i = 11; buf[i] = '\0';
    int neg = v < 0;
    unsigned u = neg ? (unsigned)(-v) : (unsigned)v;
    if (u == 0) buf[--i] = '0';
    while (u) { buf[--i] = (char)('0' + (u % 10)); u /= 10; }
    if (neg) buf[--i] = '-';
    return &buf[i];
}

static const char* find_chr(const char* s, char c) {
    for (; *s; s++) if (*s == c) return s;
    return 0;
}

// ── screens ─────────────────────────────────────────────────────────────────

enum { SCR_MENU, SCR_STATUS, SCR_ARP, SCR_PORTS, SCR_ABOUT, SCR_EXIT };

enum { M_STATUS = 1, M_ARP, M_PORTS, M_ABOUT, M_EXIT };

static int g_screen = SCR_MENU;

// Read "connected\t<IP>\n" → out; returns 1 if connected.
static int sta_ip(char* out, int max) {
    char buf[80];
    int n = wifi_sta_status(buf, (int)sizeof(buf));
    if (n <= 0) return 0;
    buf[n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1] = '\0';
    const char* tab = find_chr(buf, '\t');
    if (!tab) return 0;
    tab++;
    int i = 0;
    while (tab[i] && tab[i] != '\n' && i < max - 1) { out[i] = tab[i]; i++; }
    out[i] = '\0';
    return i > 0;
}

// gateway = our IP with the last octet replaced by "1" (the usual router).
static void derive_gw(const char* ip, char* gw, int max) {
    int last_dot = -1;
    for (int i = 0; ip[i]; i++) if (ip[i] == '.') last_dot = i;
    if (last_dot < 0) { gw[0] = '\0'; return; }
    int j = 0;
    while (j <= last_dot && j < max - 2) { gw[j] = ip[j]; j++; }
    gw[j++] = '1';
    gw[j] = '\0';
}

static void screen_status(void) {
    char ip[40];
    int up = sta_ip(ip, (int)sizeof(ip));
    ui_begin();
    ui_title("STA Status");
    if (up) {
        char line[64] = "Connected"; ui_text(line);
        int used = 0; char ipl[48] = ""; str_append(ipl, sizeof(ipl), &used, "IP: ");
        str_append(ipl, sizeof(ipl), &used, ip);
        ui_text(ipl);
        char gw[40]; derive_gw(ip, gw, (int)sizeof(gw));
        used = 0; char gwl[48] = ""; str_append(gwl, sizeof(gwl), &used, "Gateway: ");
        str_append(gwl, sizeof(gwl), &used, gw);
        ui_text(gwl);
    } else {
        ui_text("Disconnected");
        ui_text("Connect WiFi in Settings");
    }
    ui_button("< Back", 99);
    ui_end();
    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 99) g_screen = SCR_MENU;
}

static void screen_arp(void) {
    // wifi_arp_scan blocks while it pings the subnet — show a frame first.
    ui_begin();
    ui_title("ARP Scan");
    ui_text("Pinging subnet...");
    ui_text("Please wait");
    ui_end();

    static char buf[1024];
    int n = wifi_arp_scan(buf, (int)sizeof(buf));

    ui_begin();
    ui_title("ARP Scan");
    if (n <= 0) {
        ui_text("No hosts / not connected");
    } else {
        buf[n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1] = '\0';
        // buf is "IP\n" per host — show up to 6 lines.
        const char* p = buf;
        int shown = 0;
        while (*p && shown < 6) {
            char line[40]; int li = 0;
            while (*p && *p != '\n' && li < (int)sizeof(line) - 1) line[li++] = *p++;
            line[li] = '\0';
            if (*p == '\n') p++;
            if (li > 0) { ui_text(line); shown++; }
        }
        char cnt[32] = "Hosts: "; int u = 7;
        str_append(cnt, sizeof(cnt), &u, itoa_s(shown));
        ui_text(cnt);
    }
    ui_button("Rescan", 1);
    ui_button("< Back", 99);
    ui_end();
    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 99) g_screen = SCR_MENU;
    // ev==1 (Rescan) just re-enters this screen on the next loop.
}

static void screen_ports(void) {
    static const int kPorts[] = { 21, 22, 23, 53, 80, 139, 443, 445, 3389, 8080 };
    static const int kNPorts = (int)(sizeof(kPorts) / sizeof(kPorts[0]));

    char ip[40];
    if (!sta_ip(ip, (int)sizeof(ip))) {
        ui_begin();
        ui_title("Port Scan");
        ui_text("Not connected");
        ui_button("< Back", 99);
        ui_end();
        int ev = ui_wait_event();
        if (ev == EV_BACK || ev == 99) g_screen = SCR_MENU;
        return;
    }
    char gw[40]; derive_gw(ip, gw, (int)sizeof(gw));

    ui_begin();
    ui_title("Port Scan");
    { char l[48] = "Target: "; int u = 8; str_append(l, sizeof(l), &u, gw); ui_text(l); }
    ui_text("Scanning ports...");
    ui_end();

    // Probe each port; collect the open ones.
    char open_list[80] = ""; int ou = 0; int open_count = 0;
    for (int i = 0; i < kNPorts; i++) {
        if (wifi_tcp_probe(gw, kPorts[i], 400) == 0) {
            if (open_count > 0) str_append(open_list, sizeof(open_list), &ou, ", ");
            str_append(open_list, sizeof(open_list), &ou, itoa_s(kPorts[i]));
            open_count++;
        }
    }

    ui_begin();
    ui_title("Port Scan");
    { char l[48] = "Target: "; int u = 8; str_append(l, sizeof(l), &u, gw); ui_text(l); }
    if (open_count == 0) {
        ui_text("No open ports found");
    } else {
        char c[32] = "Open ("; int u = 6;
        str_append(c, sizeof(c), &u, itoa_s(open_count));
        str_append(c, sizeof(c), &u, "):");
        ui_text(c);
        ui_text(open_list);
    }
    ui_button("Rescan", 1);
    ui_button("< Back", 99);
    ui_end();
    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 99) g_screen = SCR_MENU;
}

static void screen_about(void) {
    ui_begin();
    ui_title("About");
    ui_text("Network Tools");
    ui_text("Version " APP_VERSION);
    ui_text("LAN utils over STA");
    ui_button("< Back", 99);
    ui_end();
    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == 99) g_screen = SCR_MENU;
}

static void screen_menu(void) {
    ui_begin();
    ui_title("Network Tools");
    ui_button("STA Status", M_STATUS);
    ui_button("ARP Scan",   M_ARP);
    ui_button("Port Scan",  M_PORTS);
    ui_button("About",      M_ABOUT);
    ui_button("Exit",       M_EXIT);
    ui_end();
    int ev = ui_wait_event();
    if (ev == EV_BACK || ev == M_EXIT) { g_screen = SCR_EXIT; return; }
    switch (ev) {
        case M_STATUS: g_screen = SCR_STATUS; break;
        case M_ARP:    g_screen = SCR_ARP;    break;
        case M_PORTS:  g_screen = SCR_PORTS;  break;
        case M_ABOUT:  g_screen = SCR_ABOUT;  break;
    }
}

__attribute__((export_name("main")))
int main(void) {
    nema_log("info", "NetworkTools", "started v" APP_VERSION);
    while (g_screen != SCR_EXIT) {
        switch (g_screen) {
            case SCR_MENU:   screen_menu();   break;
            case SCR_STATUS: screen_status(); break;
            case SCR_ARP:    screen_arp();    break;
            case SCR_PORTS:  screen_ports();  break;
            case SCR_ABOUT:  screen_about();  break;
        }
    }
    return 0;
}
