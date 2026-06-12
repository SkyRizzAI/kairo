#pragma once
#include "nema/hal/driver.h"
#include <string>

namespace nema {

struct HttpResponse {
    int         status = 0;     // HTTP status code, 0 = transport error
    std::string body;          // response body (truncated to a sane cap)
    bool ok() const { return status >= 200 && status < 300; }
};

// Blocking HTTP client. get() BLOCKS and MUST be called from a TaskRunner
// worker thread — never the UI/app loop. Platform impls: esp_http_client on
// device, curl via popen() on host. This keeps networked apps platform-agnostic.
struct IHttpClient : IDriver {
    const char* name() const override { return "HttpClient"; }
    DriverKind  kind() const override { return DriverKind::Other; }

    // HTTPS GET. Returns status + body. insecure=true skips cert validation
    // (fine for public read-only APIs in this prototype).
    virtual HttpResponse get(const char* url, bool insecure = true) = 0;
};

} // namespace nema
