// Esp32NetSockets — INetSockets via lwip BSD sockets (Plan 91 Stage 2).
#include "nema/esp32/esp32_net_sockets.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

namespace nema {

static bool bindAndNonblock(int fd, uint16_t port) {
    struct sockaddr_in sa = {};
    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port        = htons(port);
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) < 0)
        return false;
    int fl = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    return true;
}

int Esp32NetSockets::alloc(int fd) {
    for (int i = 0; i < kMax; ++i)
        if (slots_[i] < 0) { slots_[i] = fd; return i + 1; }
    ::close(fd);   // table full
    return -1;
}
int Esp32NetSockets::fdOf(int h) const {
    return (h >= 1 && h <= kMax) ? slots_[h - 1] : -1;
}
void Esp32NetSockets::freeSlot(int h) {
    if (h < 1 || h > kMax) return;
    if (slots_[h - 1] >= 0) { ::close(slots_[h - 1]); slots_[h - 1] = -1; }
}

void Esp32NetSockets::stop() { for (int i = 0; i < kMax; ++i) freeSlot(i + 1); }

int Esp32NetSockets::udpOpen(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return -1;
    if (!bindAndNonblock(fd, port)) { ::close(fd); return -1; }
    return alloc(fd);
}

int Esp32NetSockets::udpRecv(int h, uint8_t* buf, int max,
                             uint32_t& fromIp, uint16_t& fromPort) {
    int fd = fdOf(h);
    if (fd < 0) return -1;
    struct sockaddr_in src = {};
    socklen_t sl = sizeof(src);
    int n = ::recvfrom(fd, buf, max, 0,
                       reinterpret_cast<struct sockaddr*>(&src), &sl);
    if (n <= 0) return 0;   // EWOULDBLOCK → "none pending"
    fromIp   = ntohl(src.sin_addr.s_addr);
    fromPort = ntohs(src.sin_port);
    return n;
}

int Esp32NetSockets::udpSend(int h, uint32_t toIp, uint16_t toPort,
                             const uint8_t* buf, int len) {
    int fd = fdOf(h);
    if (fd < 0) return -1;
    struct sockaddr_in dst = {};
    dst.sin_family      = AF_INET;
    dst.sin_addr.s_addr = htonl(toIp);
    dst.sin_port        = htons(toPort);
    return ::sendto(fd, buf, len, 0,
                    reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));
}

int Esp32NetSockets::tcpListen(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (!bindAndNonblock(fd, port) || ::listen(fd, 4) < 0) {
        ::close(fd); return -1;
    }
    return alloc(fd);
}

int Esp32NetSockets::tcpAccept(int listenHandle) {
    int fd = fdOf(listenHandle);
    if (fd < 0) return -1;
    int c = ::accept(fd, nullptr, nullptr);
    if (c < 0) return -1;   // none pending (non-blocking)
    int fl = ::fcntl(c, F_GETFL, 0);
    ::fcntl(c, F_SETFL, fl | O_NONBLOCK);
    return alloc(c);
}

int Esp32NetSockets::tcpRecv(int h, uint8_t* buf, int max) {
    int fd = fdOf(h);
    if (fd < 0) return -1;
    int n = ::recv(fd, buf, max, 0);
    if (n == 0) return -1;   // peer closed
    if (n < 0)  return 0;    // EWOULDBLOCK → none pending
    return n;
}

int Esp32NetSockets::tcpSend(int h, const uint8_t* buf, int len) {
    int fd = fdOf(h);
    if (fd < 0) return -1;
    return ::send(fd, buf, len, 0);
}

void Esp32NetSockets::closeHandle(int h) { freeSlot(h); }

} // namespace nema
