#pragma once
#include "nema/hal/driver.h"
#include <string>

namespace nema {

struct HttpResponse {
    int         status = 0;     // HTTP status code, 0 = transport error
    std::string headers;       // raw response headers ("Name: Value\n" per line)
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

    // HTTPS POST with a request body and Content-Type. Same blocking contract as
    // get(). Default = "unsupported" (status 0) so platforms without a POST impl
    // don't break the link; real transports override it.
    virtual HttpResponse post(const char* url, const char* body,
                              const char* contentType, bool insecure = true) {
        (void)url; (void)body; (void)contentType; (void)insecure;
        return {};
    }

    // General request — any verb, raw "Name: Value\n" request-header block, and a
    // body. The curl/fetch-style escape hatch. Default delegates to get()/post()
    // for the common verbs so partial transports still work; full transports
    // override it to honour arbitrary methods + headers.
    virtual HttpResponse request(const char* method, const char* url,
                                 const char* headers, const char* body,
                                 bool insecure = true) {
        (void)headers;
        if (method && (method[0] == 'P' || method[0] == 'p'))  // POST/PUT/PATCH
            return post(url, body ? body : "", "application/octet-stream", insecure);
        return get(url, insecure);
    }
};

} // namespace nema
