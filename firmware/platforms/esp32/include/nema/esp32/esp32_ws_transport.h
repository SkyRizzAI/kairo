#pragma once
#include "nema/link/transport.h"
#include <cstdint>
#include <cstddef>

namespace nema {

class Logger;

// Esp32WsTransport — PLP over WebSocket (Plan 75). The device runs an
// esp_http_server with a binary WebSocket endpoint at "/plp"; Forge web (browser
// WebSocket) connects over Wi-Fi and speaks the same PLP frames as USB/BLE.
//
// Lifecycle is gated: begin() when the device is online + remote enabled, end()
// when Wi-Fi drops. Single client (one session) for now (multi-session = Plan 45).
// All esp_http_server types are hidden behind void* so the header stays clean.
class Esp32WsTransport : public ILinkTransport {
public:
    void init(Logger* log) { log_ = log; }
    void begin(uint16_t port = 8477);   // start httpd + register "/plp"
    void end();                          // stop httpd
    bool running() const { return server_ != nullptr; }

    // ILinkTransport
    bool   send(const uint8_t* data, size_t len) override;
    void   onRecv(RecvFn fn, void* user) override { recv_ = fn; recvUser_ = user; }
    bool   isConnected() const override { return server_ != nullptr && clientFd_ >= 0; }
    size_t mtu() const override { return 1024; }

    // Called from the static esp_http_server handler. Takes httpd_req_t* as void*,
    // returns esp_err_t as int. Keeps ESP-IDF types out of the header.
    int onWsRequest(void* req);

private:
    void*  server_    = nullptr;   // httpd_handle_t
    int    clientFd_  = -1;        // socket fd of the connected WS client
    RecvFn recv_      = nullptr;
    void*  recvUser_  = nullptr;
    Logger* log_      = nullptr;
};

} // namespace nema
