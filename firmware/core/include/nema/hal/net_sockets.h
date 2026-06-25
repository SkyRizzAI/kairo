#pragma once
#include "nema/hal/driver.h"
#include <cstdint>

namespace nema {

// INetSockets — generic UDP/TCP socket primitives for apps (Plan 91 Stage 2).
//
// The kernel exposes raw, non-blocking sockets; apps build whatever protocol
// they need on top (DNS, HTTP, …). The kernel never knows what a socket is for
// — e.g. a "captive portal" is just a soft-AP + a UDP:53 responder + a TCP:80
// server, all composed in the app.
//
// Handles are small positive ints (1..N); 0/-1 mean "no handle / error". All
// calls are NON-BLOCKING so an app can poll them from its UI loop without
// stalling. Platforms without a TCP/IP stack (e.g. the WASM simulator) simply
// don't register an INetSockets, and the host bindings return -1.
struct INetSockets : IDriver {
    virtual ~INetSockets() = default;

    // ── UDP ────────────────────────────────────────────────────────────────
    // Open a non-blocking UDP socket bound to 0.0.0.0:port. → handle or -1.
    virtual int  udpOpen(uint16_t port) = 0;
    // Receive one datagram. Writes the sender's IPv4 (host order) + port into
    // fromIp/fromPort. Returns bytes (>0), 0 if none pending, -1 on error.
    virtual int  udpRecv(int h, uint8_t* buf, int max,
                         uint32_t& fromIp, uint16_t& fromPort) = 0;
    // Send a datagram to toIp:toPort (toIp host order). Returns bytes or -1.
    virtual int  udpSend(int h, uint32_t toIp, uint16_t toPort,
                         const uint8_t* buf, int len) = 0;

    // ── TCP (server) ─────────────────────────────────────────────────────────
    // Listen on 0.0.0.0:port (non-blocking, SO_REUSEADDR). → handle or -1.
    virtual int  tcpListen(uint16_t port) = 0;
    // Accept a pending connection on a listen handle. → client handle or -1
    // (also -1 when none pending — poll again next tick).
    virtual int  tcpAccept(int listenHandle) = 0;
    // Receive from a client handle. >0 bytes, 0 if none pending, -1 if closed.
    virtual int  tcpRecv(int h, uint8_t* buf, int max) = 0;
    // Send to a client handle. Returns bytes or -1.
    virtual int  tcpSend(int h, const uint8_t* buf, int len) = 0;

    // Close any handle (udp / tcp listen / tcp client).
    virtual void closeHandle(int h) = 0;
};

} // namespace nema
