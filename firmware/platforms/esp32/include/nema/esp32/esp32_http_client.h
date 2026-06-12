#pragma once
#include "nema/hal/http_client.h"
#include "nema/service.h"

namespace nema {

class Runtime;
class Logger;

// ESP32 HTTP client over esp_http_client (+esp-tls). Blocking; worker-thread only.
class Esp32HttpClient : public IHttpClient, public IService {
public:
    const char* name() const override { return "Esp32HttpClient"; }

    void onRegister(Runtime& rt) override;

    HttpResponse get(const char* url, bool insecure = true) override;

    void start() override {}
    void stop()  override {}

private:
    Logger* log_ = nullptr;
};

} // namespace nema
