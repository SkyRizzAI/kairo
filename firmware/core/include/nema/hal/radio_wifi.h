#pragma once
#include "nema/hal/driver.h"
#include "nema/message_queue.h"
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace nema {

// One AP found during a raw radio scan.
struct RadioScanResult {
    char    bssid[18] = {};  // "AA:BB:CC:DD:EE:FF\0"
    char    ssid[33]  = {};
    uint8_t channel   = 0;
    int8_t  rssi      = 0;   // dBm
    char    auth[16]  = {};  // "open" | "wpa" | "wpa2" | "wpa3"
};

// IRadioWifi — raw and thick WiFi radio access (Plan 87 Fase 4 + 5).
//
// This is NOT the cooked STA driver (IWifiDriver). It exposes the radio chip
// directly: passive scan, monitor mode, frame injection, and thick attack
// primitives (deauth loop, beacon spam) whose timing loop runs natively on
// Core 0 — never inside the app sandbox.
//
// Access control (enforced by generated host gating prologues):
//   scan()        — @tier(benign)    — no exclusive lease
//   deauth/beacon — @tier(sensitive) — net.wifi.inject lease
//   monitor_*     — @tier(sensitive) — net.wifi.monitor lease
//   inject()      — @tier(sensitive) — net.wifi.inject lease
//
// Threading: scan() and monitorRead() are blocking — call from TaskRunner worker.
// All other methods are non-blocking. Native loops (deauth, beacon, promiscuous
// RX) run on a Thread pinned to Core 0 (ESP32) and never touch the WASM sandbox.
//
// Two bounded queues (both shared by all subclasses):
//   monitorQ_  (128 slots) — raw 802.11 frames; producer = native RX callback,
//                            consumer = monitorRead(). Full → frame dropped.
//   eventQ_    (64 slots)  — matured JSON events from thick loops; consumer = waitEvent().
struct IRadioWifi : IDriver {
    virtual ~IRadioWifi() = default;

    // ── Cooked scan (benign — no exclusive lease required) ────────────────────
    // Blocking — runs on TaskRunner worker. Returns empty on error.
    virtual std::vector<RadioScanResult> scan() = 0;

    // ── Monitor mode (net.wifi.monitor lease) ─────────────────────────────────
    // monitorOpen: set channel + enable promiscuous capture.
    virtual bool monitorOpen(uint8_t /*channel*/) { return false; }
    virtual void monitorClose() {}

    // monitorRead: drain one raw 802.11 frame (up to max bytes) from the ring.
    // Blocks up to timeoutMs. Returns byte count written; 0 on timeout or error.
    // Subclasses push frames via pushFrame() — no need to override monitorRead.
    virtual int monitorRead(uint8_t* out, uint32_t max, uint32_t timeoutMs) {
        std::vector<uint8_t> frame;
        if (!monitorQ_.receive(frame, timeoutMs == 0 ? 1 : timeoutMs)) return 0;
        uint32_t n = frame.size() < max ? static_cast<uint32_t>(frame.size()) : max;
        std::memcpy(out, frame.data(), n);
        return static_cast<int>(n);
    }

    // ── Frame injection (net.wifi.inject lease) ────────────────────────────────
    virtual bool inject(uint8_t /*ch*/, const uint8_t* /*frame*/, size_t /*len*/) { return false; }

    // ── Thick primitives — loop runs natively (net.wifi.inject lease) ─────────
    // Start continuous deauth at ~10 Hz (firmware Core 0). App gets events via waitEvent().
    virtual bool deauthStart(std::string_view /*bssid*/, uint8_t /*channel*/) { return false; }
    virtual bool deauthStop()                                                  { return true; }

    // Start beacon spam (multiple fake SSIDs, firmware-native loop).
    virtual bool beaconSpamStart(const std::vector<std::string>& /*ssids*/) { return false; }
    virtual bool beaconSpamStop()                                            { return true; }

    // Start probe request flood at ~20 Hz (firmware Core 0).
    // ssid="" → wildcard probes (discover hidden APs); ssid set → targeted.
    // App gets events ("probe_sent") via waitEvent().
    virtual bool probeFloodStart(std::string_view /*ssid*/, uint8_t /*channel*/) { return false; }
    virtual bool probeFloodStop()                                                  { return true; }

    // ── New attack primitives (net.wifi.inject lease) ─────────────────────────
    // Set radio MAC address. mac="AA:BB:CC:DD:EE:FF"; empty → no-op.
    virtual bool setMac(std::string_view /*mac*/) { return false; }

    // Karma: respond to every probe request with a matching fake AP.
    // Events ("karma_hit" + {ssid,sta}) pushed via waitEvent().
    virtual bool karmaStart() { return false; }
    virtual bool karmaStop()  { return true; }

    // Evil portal: open soft-AP named ssid, DNS-hijack all queries to 192.168.4.1,
    // serve html as captive HTTP portal (nullptr → built-in page).
    // Events ("ep_creds" + {data}) pushed when a form is POSTed.
    virtual bool evilPortalStart(std::string_view /*ssid*/,
                                  const char* /*html*/    = nullptr,
                                  size_t      /*htmlLen*/ = 0) { return false; }
    virtual bool evilPortalStop() { return true; }

    // ── Network tool primitives ────────────────────────────────────────────────
    // Check STA connection. Writes "connected\t<IP>\n" or "disconnected\n".
    // Returns bytes written.
    virtual int staStatus(char* out, uint32_t max) {
        static const char kDis[] = "disconnected\n";
        if (max < sizeof(kDis)) return 0;
        std::memcpy(out, kDis, sizeof(kDis) - 1); out[sizeof(kDis)-1] = '\0';
        return static_cast<int>(sizeof(kDis) - 1);
    }

    // ARP/ping scan: discover live hosts on current subnet (blocking, STA must
    // be connected). Writes "IP\n" per host. Returns bytes written; 0=not connected.
    virtual int arpScan(char* /*out*/, uint32_t /*max*/) { return 0; }

    // TCP port probe: connect to host:port within timeoutMs.
    // Returns 0=open, -1=closed/refused/timeout. Blocking.
    virtual int tcpProbe(std::string_view /*host*/, uint16_t /*port*/,
                         uint32_t /*timeoutMs*/ = 3000) { return -1; }

    // ── Monitor frame ring (Fase 5) ───────────────────────────────────────────
    // pushFrame: called by promiscuous RX callbacks to enqueue raw frames.
    // Thread-safe and non-blocking. Drops the frame if the ring is full.
    void pushFrame(const uint8_t* data, size_t len) {
        if (len == 0 || len > 2500) return;  // sanity-clamp
        monitorQ_.send(std::vector<uint8_t>(data, data + len));
    }

    // ── Matured event queue (thick loops → app) ────────────────────────────────
    // waitEvent: block up to timeoutMs for next JSON event from deauth/beacon loops.
    // Returns empty on timeout. Call from TaskRunner worker — never the UI thread.
    virtual std::vector<uint8_t> waitEvent(uint32_t timeoutMs) {
        std::vector<uint8_t> out;
        eventQ_.receive(out, timeoutMs == 0 ? 1 : timeoutMs);
        return out;
    }

    // pushEvent: called by native loops on Core 0. Thread-safe; drops if full.
    virtual void pushEvent(const char* type, const char* data = "{}") {
        std::string s;
        s.reserve(32 + std::strlen(type) + std::strlen(data));
        s += "{\"type\":\""; s += type; s += "\",\"data\":"; s += data; s += '}';
        eventQ_.send(std::vector<uint8_t>(s.begin(), s.end()));
    }

private:
    // 128 raw-frame slots — full → frame dropped, radio never stalls.
    MessageQueue<std::vector<uint8_t>> monitorQ_{128};
    MessageQueue<std::vector<uint8_t>> eventQ_{64};
};

} // namespace nema
