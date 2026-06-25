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

// IRadioWifi — raw WiFi radio access (Plan 87 Fase 4 + 5; Plan 91).
//
// This is NOT the cooked STA driver (IWifiDriver). It exposes the radio chip
// directly as GENERIC MECHANISM only: passive scan, monitor mode, frame
// injection, a soft AP, and MAC override. There are NO attack/app verbs here —
// deauth, beacon spam, karma, and captive portals are app policy, built from
// these primitives in the app sandbox (Plan 91).
//
// Access control (enforced by generated host gating prologues):
//   scan()        — @tier(benign)    — no exclusive lease
//   monitor_*     — @tier(sensitive) — net.wifi.monitor lease
//   inject()/ap   — @tier(sensitive) — net.wifi.inject lease
//
// Threading: scan() and monitorRead() are blocking — call from TaskRunner worker.
// All other methods are non-blocking. The promiscuous RX callback pushes frames
// into monitorQ_ and never touches the WASM sandbox.
//
//   monitorQ_  (128 slots) — raw 802.11 frames; producer = native RX callback,
//                            consumer = monitorRead(). Full → frame dropped.
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

    // NOTE: deauth / beacon-spam / probe-flood / karma are NOT radio methods.
    // They are app policy — apps build those frames and inject() them in their own
    // loop. The HAL exposes only mechanism (scan/monitor/inject/setMac). Plan 91.

    // Set radio MAC address. mac="AA:BB:CC:DD:EE:FF"; empty → no-op.
    virtual bool setMac(std::string_view /*mac*/) { return false; }

    // ── Soft AP (generic — apps compose captive portals from AP + sockets) ────
    // Bring up a soft AP broadcasting `ssid` on `channel` (open auth if open=true).
    // Creates the AP netif + DHCP server (192.168.4.1). Suspends STA while up.
    virtual bool apStart(std::string_view /*ssid*/, uint8_t /*channel*/,
                         bool /*open*/) { return false; }
    // Tear the AP down and restore STA mode.
    virtual bool apStop() { return false; }

    // NOTE: there is no "evil portal" in the kernel. A captive portal is just
    // apStart() + a UDP:53 + a TCP:80 server, all composed in the app from the
    // generic socket primitives (INetSockets). Plan 91 Stage 2.

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

private:
    // 128 raw-frame slots — full → frame dropped, radio never stalls.
    MessageQueue<std::vector<uint8_t>> monitorQ_{128};
};

} // namespace nema
