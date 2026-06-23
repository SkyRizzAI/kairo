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

// ── wifi_deauth_start(bssid, channel) → 0 ok / -1 err ──────────────────────

m3ApiRawFunction(wasm_wifi_deauth_start) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, bssidOff);
    m3ApiGetArg(int32_t,  ch);

    if (checkPerm(runtime, "net.wifi.inject") != 1) m3ApiReturn(-1);
    if (!acquireLease(runtime, "net.wifi.inject", s_injectHandle)) m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(-1);

    std::string bssid;
    if (!readCStr(runtime, bssidOff, bssid)) m3ApiReturn(-1);

    m3ApiReturn(r->deauthStart(bssid, (uint8_t)ch) ? 0 : -1);
}

// ── wifi_deauth_stop() → 0 ──────────────────────────────────────────────────

m3ApiRawFunction(wasm_wifi_deauth_stop) {
    m3ApiReturnType(int32_t);
    IRadioWifi* r = radio(runtime);
    if (r) r->deauthStop();
    releaseLease(runtime, s_injectHandle);
    m3ApiReturn(0);
}

// ── wifi_beacon_spam_start(ssids_buf, count) → 0 ok / -1 err ───────────────
// ssids_buf: NUL-separated SSID strings; count: number of SSIDs.

m3ApiRawFunction(wasm_wifi_beacon_spam_start) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, bufsOff);
    m3ApiGetArg(int32_t,  count);

    if (checkPerm(runtime, "net.wifi.inject") != 1) m3ApiReturn(-1);
    if (!acquireLease(runtime, "net.wifi.inject", s_injectHandle)) m3ApiReturn(-1);
    IRadioWifi* r = radio(runtime);
    if (!r || count <= 0) m3ApiReturn(-1);

    uint32_t memSz = 0;
    uint8_t* base  = m3_GetMemory(runtime, &memSz, 0);
    if (!base || bufsOff >= memSz) m3ApiReturn(-1);

    std::vector<std::string> ssids;
    ssids.reserve((size_t)count);
    uint32_t cur = bufsOff;
    for (int i = 0; i < count && cur < memSz; i++) {
        uint32_t end = cur;
        while (end < memSz && base[end]) end++;
        if (end >= memSz) break;
        ssids.emplace_back(reinterpret_cast<const char*>(base + cur), end - cur);
        cur = end + 1;
    }

    m3ApiReturn(r->beaconSpamStart(ssids) ? 0 : -1);
}

// ── wifi_beacon_spam_stop() → 0 ─────────────────────────────────────────────

m3ApiRawFunction(wasm_wifi_beacon_spam_stop) {
    m3ApiReturnType(int32_t);
    IRadioWifi* r = radio(runtime);
    if (r) r->beaconSpamStop();
    releaseLease(runtime, s_injectHandle);
    m3ApiReturn(0);
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
    releaseLease(runtime, s_monitorHandle);
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

// ── wifi_wait_event(out, max, timeout_ms) → bytes written ───────────────────

m3ApiRawFunction(wasm_wifi_wait_event) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t,  maxArg);
    m3ApiGetArg(int32_t,  timeoutMs);

    if (maxArg <= 0) m3ApiReturn(0);

    IRadioWifi* r = radio(runtime);
    if (!r) m3ApiReturn(0);

    auto ev = r->waitEvent(timeoutMs > 0 ? (uint32_t)timeoutMs : 1u);
    if (ev.empty()) m3ApiReturn(0);

    m3ApiReturn(writeBuf(runtime, outOff, maxArg,
                         ev.data(), (int)ev.size()));
}

} // anon namespace

void linkWifiImports(IM3Module mod) {
    auto link = [mod](const char* fn, const char* sig, M3RawCall cb) {
        m3_LinkRawFunction(mod, "wifi", fn, sig, cb);
    };
    link("wifi_scan",             "i(*i)",   &wasm_wifi_scan);
    link("wifi_deauth_start",     "i(*i)",   &wasm_wifi_deauth_start);
    link("wifi_deauth_stop",      "i()",     &wasm_wifi_deauth_stop);
    link("wifi_beacon_spam_start","i(*i)",   &wasm_wifi_beacon_spam_start);
    link("wifi_beacon_spam_stop", "i()",     &wasm_wifi_beacon_spam_stop);
    link("wifi_monitor_open",     "i(i)",    &wasm_wifi_monitor_open);
    link("wifi_monitor_read",     "i(*ii)",  &wasm_wifi_monitor_read);
    link("wifi_monitor_close",    "v()",     &wasm_wifi_monitor_close);
    link("wifi_inject",           "i(i*i)",  &wasm_wifi_inject);
    link("wifi_wait_event",       "i(*ii)",  &wasm_wifi_wait_event);
}

} // namespace nema
