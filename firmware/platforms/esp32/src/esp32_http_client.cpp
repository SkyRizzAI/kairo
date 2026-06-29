#include "nema/system/capabilities.h"
#include "nema/esp32/esp32_http_client.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include "nema/system/capability_registry.h"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <string>
#include <cstring>
#include <strings.h>   // strcasecmp

namespace nema {

void Esp32HttpClient::onRegister(Runtime& rt) {
    log_ = &rt.log();
    rt.container().registerService(this);
    rt.container().registerAs<IHttpClient>(this);
    rt.capabilities().add(caps::NetHttp);
}

// Response collector — esp_http_client streams the body in chunks and reports
// each response header as it arrives.
struct CollectCtx { std::string* body; std::string* headers; size_t cap; };

static esp_err_t evtHandler(esp_http_client_event_t* e) {
    if (!e->user_data) return ESP_OK;
    auto* ctx = static_cast<CollectCtx*>(e->user_data);
    if (e->event_id == HTTP_EVENT_ON_DATA) {
        if (ctx->body->size() < ctx->cap)
            ctx->body->append((const char*)e->data, e->data_len);
    } else if (e->event_id == HTTP_EVENT_ON_HEADER && ctx->headers) {
        if (ctx->headers->size() < ctx->cap) {
            ctx->headers->append(e->header_key);
            ctx->headers->append(": ");
            ctx->headers->append(e->header_value);
            ctx->headers->push_back('\n');
        }
    }
    return ESP_OK;
}

// Map an HTTP verb string to esp_http_client's method enum (defaults to GET).
static esp_http_client_method_t methodOf(const char* m) {
    if (!m) return HTTP_METHOD_GET;
    if (!strcasecmp(m, "POST"))   return HTTP_METHOD_POST;
    if (!strcasecmp(m, "PUT"))    return HTTP_METHOD_PUT;
    if (!strcasecmp(m, "PATCH"))  return HTTP_METHOD_PATCH;
    if (!strcasecmp(m, "DELETE")) return HTTP_METHOD_DELETE;
    if (!strcasecmp(m, "HEAD"))   return HTTP_METHOD_HEAD;
    return HTTP_METHOD_GET;
}

// Split a raw "Name: Value\n" block and register each as a request header.
static void applyHeaders(esp_http_client_handle_t h, const char* headers) {
    if (!headers || !*headers) return;
    std::string blk(headers);
    size_t pos = 0;
    while (pos < blk.size()) {
        size_t nl = blk.find('\n', pos);
        std::string line = blk.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            size_t vs = colon + 1;
            while (vs < line.size() && (line[vs] == ' ' || line[vs] == '\t')) vs++;
            std::string val = line.substr(vs);
            if (!key.empty() && !val.empty() && val.back() == '\r') val.pop_back();
            if (!key.empty()) esp_http_client_set_header(h, key.c_str(), val.c_str());
        }
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
}

HttpResponse Esp32HttpClient::get(const char* url, bool insecure) {
    return request("GET", url, nullptr, nullptr, insecure);
}

HttpResponse Esp32HttpClient::post(const char* url, const char* body,
                                   const char* contentType, bool insecure) {
    std::string hdr;
    if (contentType && *contentType) { hdr = "Content-Type: "; hdr += contentType; }
    return request("POST", url, hdr.empty() ? nullptr : hdr.c_str(), body, insecure);
}

HttpResponse Esp32HttpClient::request(const char* method, const char* url,
                                      const char* headers, const char* body,
                                      bool insecure) {
    HttpResponse r;
    CollectCtx ctx{ &r.body, &r.headers, 8192 };

    esp_http_client_config_t cfg = {};
    cfg.url               = url;
    cfg.method            = methodOf(method);
    cfg.event_handler     = evtHandler;
    cfg.user_data         = &ctx;
    cfg.timeout_ms        = 10000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;   // verified TLS via IDF bundle
    cfg.skip_cert_common_name_check = insecure;

    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) return r;
    applyHeaders(h, headers);
    if (body && *body)
        esp_http_client_set_post_field(h, body, (int)std::strlen(body));
    esp_err_t err = esp_http_client_perform(h);
    if (err == ESP_OK) r.status = esp_http_client_get_status_code(h);
    else if (log_)     log_->warn("Esp32HttpClient", "request failed", {{"method", method ? method : "?"}});
    esp_http_client_cleanup(h);
    return r;
}

} // namespace nema
