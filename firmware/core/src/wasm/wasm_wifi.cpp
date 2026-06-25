// Plan 87 Fase 8 — wifi.* host imports for WASM apps.
//
// Exposes IRadioWifi to the wasm3 sandbox via the "wifi" import module.
// Each binding resolves IRadioWifi from the ServiceContainer at call time —
// if the device has no radio, the function returns -1/0 and logs nothing.
//
// Scan result wire format (newline-separated):
//   BSSID|SSID|channel|rssi|auth\n
// e.g.: "AA:BB:CC:DD:EE:FF|MyNet|6|-72|wpa2\n"
//
// Beacon spam: ssids_buf is a NUL-separated list of SSID strings, count
// is the number of SSIDs.  e.g. "FakeNet1\0FakeNet2\0FakeNet3\0"
//
// Monitor/event read: returns raw bytes.  0 = timeout, >0 = bytes written.

#include "nema/wasm/wasm_engine.h"
#include "nema/proc/process_context.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/hal/radio_wifi.h"
#include "nema/hal/net_sockets.h"
#include "nema/service/service_container.h"
#include "nema/services/permission_service.h"
#include "nema/services/resource_broker.h"
#include "wasm3.h"
#include "m3_env.h"
#include <cstring>
#include <string>
#include <vector>

namespace nema {
namespace {

// ── Helpers (local copies — wasm_nema.cpp's helpers are in its anon ns) ────

static WasmHostCtx* hostOf(IM3Runtime rt) {
    return static_cast<WasmHostCtx*>(m3_GetUserData(rt));
}

static bool readCStr(IM3Runtime rt, uint32_t off, std::string& out) {
    uint32_t sz = 0;
    uint8_t* base = m3_GetMemory(rt, &sz, 0);
    if (!base || off >= sz) return false;
    uint32_t end = off;
    while (end < sz && base[end]) end++;
    if (end >= sz) return false;
    out.assign(reinterpret_cast<const char*>(base + off), end - off);
    return true;
}

// Write src into guest memory at off (up to cap bytes incl. NUL). Returns
// bytes written (excl. NUL), or -1 on bounds failure.
static int writeBuf(IM3Runtime rt, uint32_t off, int cap, const void* src, int len) {
    if (cap <= 0) return -1;
    uint32_t sz = 0;
    uint8_t* base = m3_GetMemory(rt, &sz, 0);
    if (!base || off >= sz || off + (uint32_t)cap > sz) return -1;
    int n = len < cap - 1 ? len : cap - 1;
    std::memcpy(base + off, src, (size_t)n);
    base[off + n] = 0;
    return n;
}

// Read raw bytes from guest (no NUL termination needed).
static bool readBytes(IM3Runtime rt, uint32_t off, uint32_t len, const uint8_t*& ptr) {
    uint32_t sz = 0;
    uint8_t* base = m3_GetMemory(rt, &sz, 0);
    if (!base || off + len > sz) return false;
    ptr = base + off;
    return true;
}

static IRadioWifi* radio(IM3Runtime rt) {
    WasmHostCtx* h = hostOf(rt);
    if (!h || !h->ctx) return nullptr;
    return h->ctx->runtime().container().resolve<IRadioWifi>();
}

// Request a sensitive capability on behalf of the running WASM app.
// Blocks the app thread until the user allows or denies (GUI thread pushes
// PermissionScreen). Returns 1=granted, 2=denied. If no PermissionService
// is registered (e.g. dev build without one), falls through as granted.
static uint8_t checkPerm(IM3Runtime rt, const char* cap) {
    WasmHostCtx* h = hostOf(rt);
    if (!h || !h->ctx) return 2;
    auto& svc = h->ctx->runtime().container();
    auto* perm = svc.resolve<PermissionService>();
    if (!perm) return 1;
    return perm->request(h->appId, cap);
}

// Lease handles — held while the WASM app owns a sensitive radio mode.
// acquire() is re-entrant (same appId+cap returns existing handle), so stale
// values after a prior run are harmless: releaseAll() freed the broker state.
static uint32_t s_monitorHandle = 0;
static uint32_t s_injectHandle  = 0;

// Session takeover: when an app calls wifi_acquire() it owns the radio for its
// whole lifetime. While held, per-feature stop functions must NOT release the
// lease — otherwise every deauth/beacon/sniff stop would yield the radio back to
// the system, which reconnects STA, which re-suspends on the next feature… the
// churn that crashed NtpService and flickered the WiFi banner. Releases are
// suppressed until wifi_release() (or app exit via ResourceBroker::releaseAll).
static bool s_sessionHeld = false;

// Acquire an exclusive resource lease for `cap`. Returns true if granted or if
// no ResourceBroker is registered (dev/permissive build).
static bool acquireLease(IM3Runtime rt, const char* cap, uint32_t& handle) {
    WasmHostCtx* h = hostOf(rt);
    if (!h || !h->ctx) return false;
    auto* broker = h->ctx->runtime().container().resolve<ResourceBroker>();
    if (!broker) return true;  // no broker = permissive — let it through
    auto r = broker->acquire(h->appId, cap);
    if (!r.ok) return false;
    handle = r.value;
    return true;
}

// Release the lease stored in `handle` and zero it out.
static void releaseLease(IM3Runtime rt, uint32_t& handle) {
    if (!handle) return;
    WasmHostCtx* h = hostOf(rt);
    if (h && h->ctx) {
        auto* broker = h->ctx->runtime().container().resolve<ResourceBroker>();
        if (broker) broker->release(h->appId, handle);
    }
    handle = 0;
}

// Per-feature stop functions call this. It only really releases when the app has
// NOT taken over the radio for the session — otherwise the radio stays owned
// until wifi_release() / app exit. This is what eliminates the per-feature churn.
static void releaseLeaseUnlessSession(IM3Runtime rt, uint32_t& handle) {
    if (s_sessionHeld) return;
    releaseLease(rt, handle);
}

// ── wifi_scan(out, cap) → bytes written (newline-sep lines) ─────────────────
// Blocking — calls IRadioWifi::scan() synchronously on the app thread.

m3ApiRawFunction(wasm_wifi_scan) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t,  cap);

    if (checkPerm(runtime, "net.wifi.scan") != 1) m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(-1);

    const auto results = r->scan();

    // Serialise: "BSSID|SSID|channel|rssi|auth\n" per AP.
    std::string out;
    out.reserve(results.size() * 60);
    auto itoa7 = [](char* buf8, int v) -> const char* {
        char* tp = buf8 + 7; *tp = '\0';
        if (v == 0) { *--tp = '0'; } else { while (v > 0) { *--tp = '0' + v % 10; v /= 10; } }
        return tp;
    };
    char tmp[8];
    for (const auto& ap : results) {
        out += ap.bssid; out += '|';
        out += ap.ssid;  out += '|';
        out += itoa7(tmp, (int)ap.channel); out += '|';
        int rs = ap.rssi;
        if (rs < 0) { out += '-'; rs = -rs; }
        out += itoa7(tmp, rs); out += '|';
        out += ap.auth; out += '\n';
    }

    m3ApiReturn(writeBuf(runtime, outOff, cap, out.c_str(), (int)out.size()));
}

// ── wifi_monitor_open(channel) → 0 ok / -1 err ──────────────────────────────

m3ApiRawFunction(wasm_wifi_monitor_open) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, ch);

    if (checkPerm(runtime, "net.wifi.monitor") != 1) m3ApiReturn(-1);
    if (!acquireLease(runtime, "net.wifi.monitor", s_monitorHandle)) m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(-1);
    m3ApiReturn(r->monitorOpen((uint8_t)ch) ? 0 : -1);
}

// ── wifi_monitor_read(out, max, timeout_ms) → bytes read (0 = timeout) ──────

m3ApiRawFunction(wasm_wifi_monitor_read) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t,  max);
    m3ApiGetArg(int32_t,  timeoutMs);

    if (max <= 0) m3ApiReturn(0);

    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(0);

    uint32_t memSz = 0;
    uint8_t* base  = m3_GetMemory(runtime, &memSz, 0);
    if (!base || outOff + (uint32_t)max > memSz) m3ApiReturn(0);

    int n = r->monitorRead(base + outOff, (uint32_t)max,
                           timeoutMs > 0 ? (uint32_t)timeoutMs : 1u);
    m3ApiReturn((int32_t)n);
}

// ── wifi_monitor_close() ─────────────────────────────────────────────────────

m3ApiRawFunction(wasm_wifi_monitor_close) {
    IRadioWifi* r = radio(runtime);
    if (r) r->monitorClose();
    releaseLeaseUnlessSession(runtime, s_monitorHandle);
    m3ApiSuccess();
}

// ── wifi_inject(channel, frame, len) → 0 ok / -1 err ───────────────────────

m3ApiRawFunction(wasm_wifi_inject) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t,  ch);
    m3ApiGetArg(uint32_t, frameOff);
    m3ApiGetArg(int32_t,  len);

    if (len <= 0) m3ApiReturn(-1);
    if (checkPerm(runtime, "net.wifi.inject") != 1) m3ApiReturn(-1);
    // Acquire inject lease if not already held (e.g. deauth/spam not started yet).
    if (!s_injectHandle && !acquireLease(runtime, "net.wifi.inject", s_injectHandle))
        m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(-1);

    const uint8_t* ptr = nullptr;
    if (!readBytes(runtime, frameOff, (uint32_t)len, ptr)) m3ApiReturn(-1);

    m3ApiReturn(r->inject((uint8_t)ch, ptr, (size_t)len) ? 0 : -1);
}

// ── wifi_inject_release() → release inject lease without stopping radio loop ─
// Call this after one-shot injections (e.g. inject_badmsg, inject_sleep) that
// use wifi_inject() directly without a matching _start()/_stop() pair. Without
// this, the net.wifi.inject lease stays held until app exit, leaving the
// "Radio in use by:" banner visible in WiFi settings.

m3ApiRawFunction(wasm_wifi_inject_release) {
    releaseLeaseUnlessSession(runtime, s_injectHandle);
    m3ApiSuccess();
}

// ── wifi_set_mac(mac_str) → 0 ok / -1 err ───────────────────────────────────

m3ApiRawFunction(wasm_wifi_set_mac) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, macOff);
    if (checkPerm(runtime, "net.wifi.inject") != 1) m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(-1);
    std::string mac;
    if (!readCStr(runtime, macOff, mac)) m3ApiReturn(-1);
    m3ApiReturn(r->setMac(mac) ? 0 : -1);
}

// ── wifi_sta_status(out, max) → bytes written ────────────────────────────────

m3ApiRawFunction(wasm_wifi_sta_status) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t,  cap);
    if (cap <= 0) m3ApiReturn(0);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(0);
    uint32_t memSz = 0;
    uint8_t* base  = m3_GetMemory(runtime, &memSz, 0);
    if (!base || outOff + (uint32_t)cap > memSz) m3ApiReturn(0);
    int n = r->staStatus(reinterpret_cast<char*>(base + outOff), (uint32_t)cap);
    m3ApiReturn((int32_t)n);
}

// ── wifi_arp_scan(out, max) → bytes written (blocking ~4s) ──────────────────

m3ApiRawFunction(wasm_wifi_arp_scan) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t,  cap);
    if (cap <= 0) m3ApiReturn(0);
    if (checkPerm(runtime, "net.wifi.scan") != 1) m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(0);
    uint32_t memSz = 0;
    uint8_t* base  = m3_GetMemory(runtime, &memSz, 0);
    if (!base || outOff + (uint32_t)cap > memSz) m3ApiReturn(0);
    int n = r->arpScan(reinterpret_cast<char*>(base + outOff), (uint32_t)cap);
    m3ApiReturn((int32_t)n);
}

// ── wifi_tcp_probe(host, port, timeout_ms) → 0=open / -1=closed ─────────────

m3ApiRawFunction(wasm_wifi_tcp_probe) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, hostOff);
    m3ApiGetArg(int32_t,  port);
    m3ApiGetArg(int32_t,  timeoutMs);
    if (checkPerm(runtime, "net.wifi.scan") != 1) m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(-1);
    std::string host;
    if (!readCStr(runtime, hostOff, host)) m3ApiReturn(-1);
    int res = r->tcpProbe(host, (uint16_t)port,
                          timeoutMs > 0 ? (uint32_t)timeoutMs : 3000u);
    m3ApiReturn((int32_t)res);
}

// ── wifi_acquire() → 0 ok / -1 denied ───────────────────────────────────────
// Radio takeover: the app claims the WiFi radio for its whole lifetime. Call
// once at startup. Suspends the system WiFi connection (via the inject lease's
// exclusivity group) and suppresses per-feature lease churn until wifi_release()
// or app exit. After this, deauth/beacon/sniff/karma/portal switch instantly
// with no disconnect/reconnect between them.

m3ApiRawFunction(wasm_wifi_acquire) {
    m3ApiReturnType(int32_t);
    if (checkPerm(runtime, "net.wifi.inject") != 1) m3ApiReturn(-1);
    if (!acquireLease(runtime, "net.wifi.inject", s_injectHandle)) m3ApiReturn(-1);
    s_sessionHeld = true;
    m3ApiReturn(0);
}

// ── wifi_release() → 0 ──────────────────────────────────────────────────────
// Give the radio back to the system before the app exits (optional — app exit
// also frees it via ResourceBroker::releaseAll).

m3ApiRawFunction(wasm_wifi_release) {
    m3ApiReturnType(int32_t);
    s_sessionHeld = false;
    releaseLease(runtime, s_monitorHandle);
    releaseLease(runtime, s_injectHandle);
    m3ApiReturn(0);
}

// ── Soft AP (generic primitive; apps build the captive portal) ──────────────

m3ApiRawFunction(wasm_wifi_ap_start) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, ssidOff);
    m3ApiGetArg(int32_t,  channel);
    m3ApiGetArg(int32_t,  open);
    if (checkPerm(runtime, "net.wifi.inject") != 1) m3ApiReturn(-1);
    if (!s_injectHandle && !acquireLease(runtime, "net.wifi.inject", s_injectHandle))
        m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(-1);
    std::string ssid;
    if (!readCStr(runtime, ssidOff, ssid)) m3ApiReturn(-1);
    m3ApiReturn(r->apStart(ssid, (uint8_t)channel, open != 0) ? 0 : -1);
}

m3ApiRawFunction(wasm_wifi_ap_stop) {
    m3ApiReturnType(int32_t);
    IRadioWifi* r = radio(runtime);
    if (r) r->apStop();
    m3ApiReturn(0);
}

// ── Generic UDP/TCP sockets (apps build DNS/HTTP/etc on top) ─────────────────

static INetSockets* sockets(IM3Runtime rt) {
    WasmHostCtx* h = hostOf(rt);
    if (!h || !h->ctx) return nullptr;
    return h->ctx->runtime().container().resolve<INetSockets>();
}

m3ApiRawFunction(wasm_net_udp_open) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, port);
    if (checkPerm(runtime, "net.wifi.inject") != 1) m3ApiReturn(-1);
    INetSockets* s = sockets(runtime);
    m3ApiReturn(s ? s->udpOpen((uint16_t)port) : -1);
}

// net_udp_recv(h, buf, max, *out_ip, *out_port) → bytes / 0 none / -1 err
m3ApiRawFunction(wasm_net_udp_recv) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t,  h);
    m3ApiGetArg(uint32_t, bufOff);
    m3ApiGetArg(int32_t,  max);
    m3ApiGetArg(uint32_t, ipOff);
    m3ApiGetArg(uint32_t, portOff);
    INetSockets* s = sockets(runtime);
    if (!s || max <= 0) m3ApiReturn(-1);
    uint32_t memSz = 0;
    uint8_t* base  = m3_GetMemory(runtime, &memSz, 0);
    if (!base || bufOff + (uint32_t)max > memSz) m3ApiReturn(-1);
    uint32_t fromIp = 0; uint16_t fromPort = 0;
    int n = s->udpRecv(h, base + bufOff, max, fromIp, fromPort);
    if (n > 0) {
        uint32_t fp = fromPort;
        if (ipOff + 4 <= memSz)   std::memcpy(base + ipOff,   &fromIp, 4);
        if (portOff + 4 <= memSz) std::memcpy(base + portOff, &fp,     4);
    }
    m3ApiReturn(n);
}

m3ApiRawFunction(wasm_net_udp_send) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t,  h);
    m3ApiGetArg(uint32_t, ip);
    m3ApiGetArg(int32_t,  port);
    m3ApiGetArg(uint32_t, bufOff);
    m3ApiGetArg(int32_t,  len);
    INetSockets* s = sockets(runtime);
    if (!s || len < 0) m3ApiReturn(-1);
    const uint8_t* ptr = nullptr;
    if (!readBytes(runtime, bufOff, (uint32_t)len, ptr)) m3ApiReturn(-1);
    m3ApiReturn(s->udpSend(h, ip, (uint16_t)port, ptr, len));
}

m3ApiRawFunction(wasm_net_tcp_listen) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, port);
    if (checkPerm(runtime, "net.wifi.inject") != 1) m3ApiReturn(-1);
    INetSockets* s = sockets(runtime);
    m3ApiReturn(s ? s->tcpListen((uint16_t)port) : -1);
}

m3ApiRawFunction(wasm_net_tcp_accept) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, h);
    INetSockets* s = sockets(runtime);
    m3ApiReturn(s ? s->tcpAccept(h) : -1);
}

m3ApiRawFunction(wasm_net_tcp_recv) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t,  h);
    m3ApiGetArg(uint32_t, bufOff);
    m3ApiGetArg(int32_t,  max);
    INetSockets* s = sockets(runtime);
    if (!s || max <= 0) m3ApiReturn(-1);
    uint32_t memSz = 0;
    uint8_t* base  = m3_GetMemory(runtime, &memSz, 0);
    if (!base || bufOff + (uint32_t)max > memSz) m3ApiReturn(-1);
    m3ApiReturn(s->tcpRecv(h, base + bufOff, max));
}

m3ApiRawFunction(wasm_net_tcp_send) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t,  h);
    m3ApiGetArg(uint32_t, bufOff);
    m3ApiGetArg(int32_t,  len);
    INetSockets* s = sockets(runtime);
    if (!s || len < 0) m3ApiReturn(-1);
    const uint8_t* ptr = nullptr;
    if (!readBytes(runtime, bufOff, (uint32_t)len, ptr)) m3ApiReturn(-1);
    m3ApiReturn(s->tcpSend(h, ptr, len));
}

m3ApiRawFunction(wasm_net_close) {
    m3ApiGetArg(int32_t, h);
    INetSockets* s = sockets(runtime);
    if (s) s->closeHandle(h);
    m3ApiSuccess();
}

} // anon namespace

// Reset per-run WiFi takeover state between WASM runs so a stale takeover flag
// from a prior app can't leak into the next (broker leases themselves are freed
// by ResourceBroker::releaseAll on AppHostExited).
void resetWifiState() {
    s_sessionHeld   = false;
    s_injectHandle  = 0;
    s_monitorHandle = 0;
}

void linkWifiImports(IM3Module mod) {
    auto link = [mod](const char* fn, const char* sig, M3RawCall cb) {
        m3_LinkRawFunction(mod, "wifi", fn, sig, cb);
    };
    link("wifi_acquire",            "i()",      &wasm_wifi_acquire);
    link("wifi_release",            "i()",      &wasm_wifi_release);
    link("wifi_scan",               "i(*i)",    &wasm_wifi_scan);
    link("wifi_monitor_open",       "i(i)",     &wasm_wifi_monitor_open);
    link("wifi_monitor_read",       "i(*ii)",   &wasm_wifi_monitor_read);
    link("wifi_monitor_close",      "v()",      &wasm_wifi_monitor_close);
    link("wifi_inject",             "i(i*i)",   &wasm_wifi_inject);
    link("wifi_inject_release",     "v()",      &wasm_wifi_inject_release);
    link("wifi_set_mac",            "i(*)",     &wasm_wifi_set_mac);
    // Generic soft-AP + sockets (Plan 91): apps build captive portals from these.
    link("wifi_ap_start",           "i(*ii)",   &wasm_wifi_ap_start);
    link("wifi_ap_stop",            "i()",      &wasm_wifi_ap_stop);
    link("net_udp_open",            "i(i)",     &wasm_net_udp_open);
    link("net_udp_recv",            "i(i*i**)", &wasm_net_udp_recv);
    link("net_udp_send",            "i(iii*i)", &wasm_net_udp_send);
    link("net_tcp_listen",          "i(i)",     &wasm_net_tcp_listen);
    link("net_tcp_accept",          "i(i)",     &wasm_net_tcp_accept);
    link("net_tcp_recv",            "i(i*i)",   &wasm_net_tcp_recv);
    link("net_tcp_send",            "i(i*i)",   &wasm_net_tcp_send);
    link("net_close",               "v(i)",     &wasm_net_close);
    link("wifi_sta_status",         "i(*i)",    &wasm_wifi_sta_status);
    link("wifi_arp_scan",           "i(*i)",    &wasm_wifi_arp_scan);
    link("wifi_tcp_probe",          "i(*ii)",   &wasm_wifi_tcp_probe);
}

} // namespace nema
