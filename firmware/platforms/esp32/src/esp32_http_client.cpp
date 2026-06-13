#include "nema/system/capabilities.h"
#include "nema/esp32/esp32_http_client.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include "nema/system/capability_registry.h"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <string>

namespace nema {

void Esp32HttpClient::onRegister(Runtime& rt) {
    log_ = &rt.log();
    rt.container().registerService(this);
    rt.container().registerAs<IHttpClient>(this);
    rt.capabilities().add(caps::NetHttp);
}

// Body collector — esp_http_client streams the response in chunks.
struct CollectCtx { std::string* body; size_t cap; };

static esp_err_t evtHandler(esp_http_client_event_t* e) {
    if (e->event_id == HTTP_EVENT_ON_DATA && e->user_data) {
        auto* ctx = static_cast<CollectCtx*>(e->user_data);
        if (ctx->body->size() < ctx->cap)
            ctx->body->append((const char*)e->data, e->data_len);
    }
    return ESP_OK;
}

HttpResponse Esp32HttpClient::get(const char* url, bool insecure) {
    HttpResponse r;
    CollectCtx ctx{ &r.body, 8192 };

    esp_http_client_config_t cfg = {};
    cfg.url               = url;
    cfg.event_handler     = evtHandler;
    cfg.user_data         = &ctx;
    cfg.timeout_ms        = 10000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;   // verified TLS via IDF bundle
    cfg.skip_cert_common_name_check = insecure;

    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) return r;
    esp_err_t err = esp_http_client_perform(h);
    if (err == ESP_OK) r.status = esp_http_client_get_status_code(h);
    else if (log_)     log_->warn("Esp32HttpClient", "perform failed");
    esp_http_client_cleanup(h);
    return r;
}

} // namespace nema
