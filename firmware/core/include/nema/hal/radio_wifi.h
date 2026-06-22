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

// IRadioWifi — raw and thick WiFi radio access (Plan 87 Fase 4).
//
// This is NOT the cooked STA driver (IWifiDriver). It exposes the radio chip
// directly: passive scan, monitor mode, frame injection, and thick attack
// primitives (deauth loop, beacon spam) whose timing loop runs natively on
// Core 0 — never inside the app sandbox.
//
// Access control (enforced by generated host gating prologues):
//   scan()        — @tier(benign)    — no exclusive lease
//   deauth/beacon — @tier(sensitive) — net.wifi.inject lease
//   monitor_*     — @tier(sensitive) — net.wifi.monitor lease (Fase 5)
//   inject()      — @tier(sensitive) — net.wifi.inject lease (Fase 5)
//
// Threading: scan() and waitEvent() are blocking — call from TaskRunner worker.
// All other methods are non-blocking. The native loop (deauthLoop / beaconLoop)
// runs on a Thread pinned to Core 0 (ESP32) and never touches the WASM sandbox.
//
// Event queue: native loops push events via pushEvent(); the app's blocking
// waitEvent() drains them. The queue is bounded (64 slots) — drops on full so
// a slow app can never stall the radio.
struct IRadioWifi : IDriver {
    virtual ~IRadioWifi() = default;

    // ── Cooked scan (benign — no exclusive lease required) ────────────────────
    // Blocking — runs on TaskRunner worker. Returns empty on error.
    virtual std::vector<RadioScanResult> scan() = 0;

    // ── Monitor mode (net.wifi.monitor lease — Fase 5) ────────────────────────
    virtual bool monitorOpen(uint8_t /*channel*/)                                { return false; }
    virtual void monitorClose()                                                  {}
    // Blocking read from ring buffer, up to max bytes, with timeout (ms).
    virtual int  monitorRead(uint8_t* /*out*/, uint32_t /*max*/,
                             uint32_t /*timeoutMs*/)                             { return 0; }

    // ── Frame injection (net.wifi.inject lease — Fase 5) ──────────────────────
    virtual bool inject(uint8_t /*ch*/, const uint8_t* /*frame*/, size_t /*len*/) { return false; }

    // ── Thick primitives — loop runs natively (net.wifi.inject lease) ─────────
    // Start continuous deauth: firmware sends deauth frames at ~10 Hz on Core 0.
    // App receives events via waitEvent(); never touches the sandbox timing loop.
    virtual bool deauthStart(std::string_view /*bssid*/, uint8_t /*channel*/)   { return false; }
    virtual bool deauthStop()                                                    { return true; }

    // Start beacon spam: firmware broadcasts fake AP beacons for each SSID.
    virtual bool beaconSpamStart(const std::vector<std::string>& /*ssids*/)     { return false; }
    virtual bool beaconSpamStop()                                                { return true; }

    // ── Event queue ───────────────────────────────────────────────────────────
    // waitEvent: block up to timeoutMs for the next radio event.
    // Returns JSON bytes: {"type":"deauth_sent","data":{...}}
    // Returns empty on timeout. Call from TaskRunner worker — never the UI thread.
    virtual std::vector<uint8_t> waitEvent(uint32_t timeoutMs) {
        std::vector<uint8_t> out;
        eventQ_.receive(out, timeoutMs == 0 ? 1 : timeoutMs);
        return out;
    }

    // pushEvent: called by native loops on Core 0 to post events to the queue.
    // Thread-safe (MessageQueue is mutex-backed). Non-blocking — drops if full.
    virtual void pushEvent(const char* type, const char* data = "{}") {
        std::string s;
        s.reserve(32 + std::strlen(type) + std::strlen(data));
        s += "{\"type\":\""; s += type; s += "\",\"data\":"; s += data; s += '}';
        eventQ_.send(std::vector<uint8_t>(s.begin(), s.end()));
    }

private:
    MessageQueue<std::vector<uint8_t>> eventQ_{64};
};

} // namespace nema
