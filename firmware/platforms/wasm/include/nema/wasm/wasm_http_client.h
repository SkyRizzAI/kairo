#pragma once
#include "nema/hal/http_client.h"
#include "nema/service.h"

namespace nema {

class Runtime;

// Browser-backed HTTP client for the WASM simulator. The blocking IHttpClient
// contract is honoured with a SYNCHRONOUS XMLHttpRequest — allowed because the
// JS/WASM app code runs on a worker pthread (sync XHR is legal off the main
// thread), so no Asyncify/-sFETCH is needed. This gives simulator apps the same
// nema.net.http.* surface the device has over esp_http_client (1=1 parity).
class WasmHttpClient : public IHttpClient, public IService {
public:
    const char* name() const override { return "WasmHttpClient"; }

    void onRegister(Runtime& rt) override;

    HttpResponse get(const char* url, bool insecure = true) override;
    HttpResponse post(const char* url, const char* body,
                      const char* contentType, bool insecure = true) override;
    HttpResponse request(const char* method, const char* url,
                         const char* headers, const char* body,
                         bool insecure = true) override;

    void start() override {}
    void stop()  override {}
};

}  // namespace nema
