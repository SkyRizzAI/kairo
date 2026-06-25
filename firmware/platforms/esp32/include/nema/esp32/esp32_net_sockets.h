#pragma once
#include "nema/hal/net_sockets.h"
#include "nema/service.h"
#include <cstdint>

namespace nema {

// Esp32NetSockets — INetSockets via lwip BSD sockets (Plan 91 Stage 2).
// Generic non-blocking UDP/TCP primitives so apps build their own DNS/HTTP
// servers (e.g. a captive portal). Handles are 1-based indices into a fixed
// table; the kernel never knows what the sockets are for.
class Esp32NetSockets : public INetSockets, public IService {
public:
    const char* name() const override { return "Esp32NetSockets"; }
    DriverKind  kind() const override { return DriverKind::Other; }
    void start() override {}
    void stop()  override;

    int  udpOpen(uint16_t port) override;
    int  udpRecv(int h, uint8_t* buf, int max,
                 uint32_t& fromIp, uint16_t& fromPort) override;
    int  udpSend(int h, uint32_t toIp, uint16_t toPort,
                 const uint8_t* buf, int len) override;
    int  tcpListen(uint16_t port) override;
    int  tcpAccept(int listenHandle) override;
    int  tcpRecv(int h, uint8_t* buf, int max) override;
    int  tcpSend(int h, const uint8_t* buf, int len) override;
    void closeHandle(int h) override;

private:
    static constexpr int kMax = 12;
    int  slots_[kMax] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    int  alloc(int fd);
    int  fdOf(int h) const;
    void freeSlot(int h);
};

} // namespace nema
