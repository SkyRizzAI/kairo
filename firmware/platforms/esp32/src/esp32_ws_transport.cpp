#include "nema/esp32/esp32_ws_transport.h"
#include "nema/log/logger.h"
#include <esp_http_server.h>
#include <cstdlib>
#include <cstring>
#include <string>

namespace nema {

static esp_err_t plp_ws_handler(httpd_req_t* req) {
    auto* self = static_cast<Esp32WsTransport*>(req->user_ctx);
    return static_cast<esp_err_t>(self->onWsRequest(req));
}

void Esp32WsTransport::begin(uint16_t port) {
    if (server_) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = port;
    cfg.ctrl_port        = port + 1;     // avoid clashing with other httpd instances
    cfg.max_open_sockets = 3;
    cfg.lru_purge_enable = true;
    cfg.stack_size       = 6144;

    httpd_handle_t h = nullptr;
    if (httpd_start(&h, &cfg) != ESP_OK) {
        if (log_) log_->error("WsTransport", "httpd_start failed");
        return;
    }
    httpd_uri_t uri = {};
    uri.uri          = "/plp";
    uri.method       = HTTP_GET;
    uri.handler      = plp_ws_handler;
    uri.user_ctx     = this;
    uri.is_websocket = true;
    httpd_register_uri_handler(h, &uri);

    server_ = h;
    if (log_) log_->info("WsTransport", "listening",
                         {{"port", std::to_string(port)}, {"path", "/plp"}});
}

void Esp32WsTransport::end() {
    if (!server_) return;
    httpd_stop(static_cast<httpd_handle_t>(server_));
    server_   = nullptr;
    clientFd_ = -1;
    if (log_) log_->info("WsTransport", "stopped");
}

int Esp32WsTransport::onWsRequest(void* reqv) {
    auto* req = static_cast<httpd_req_t*>(reqv);

    if (req->method == HTTP_GET) {
        // WebSocket handshake completed — remember this client.
        clientFd_ = httpd_req_to_sockfd(req);
        if (log_) log_->info("WsTransport", "client connected");
        return ESP_OK;
    }

    // Inbound data frame: first call gets the length, second the payload.
    httpd_ws_frame_t frame;
    std::memset(&frame, 0, sizeof(frame));
    frame.type = HTTPD_WS_TYPE_BINARY;
    esp_err_t r = httpd_ws_recv_frame(req, &frame, 0);
    if (r != ESP_OK) return r;
    if (frame.len == 0) return ESP_OK;

    auto* buf = static_cast<uint8_t*>(std::malloc(frame.len));
    if (!buf) return ESP_ERR_NO_MEM;
    frame.payload = buf;
    r = httpd_ws_recv_frame(req, &frame, frame.len);
    if (r == ESP_OK) {
        clientFd_ = httpd_req_to_sockfd(req);
        if (recv_) recv_(recvUser_, buf, frame.len);
    }
    std::free(buf);
    return ESP_OK;
}

bool Esp32WsTransport::send(const uint8_t* data, size_t len) {
    if (!server_ || clientFd_ < 0) return false;
    httpd_ws_frame_t frame;
    std::memset(&frame, 0, sizeof(frame));
    frame.type    = HTTPD_WS_TYPE_BINARY;
    frame.payload = const_cast<uint8_t*>(data);
    frame.len     = len;
    esp_err_t r = httpd_ws_send_frame_async(static_cast<httpd_handle_t>(server_),
                                            clientFd_, &frame);
    if (r != ESP_OK) {
        clientFd_ = -1;   // client likely gone — wait for the next connection
        return false;
    }
    return true;
}

} // namespace nema
