#include "nema/wasm/wasm_http_client.h"

#include <emscripten.h>
#include <cstdlib>
#include <string>

namespace nema {

// Run a synchronous XHR on the calling (worker) thread and return a malloc'd C
// string the caller must free, formatted as:
//   "<status>\x1f<responseHeaders>\x1f<responseBody>"
// 0x1F (unit separator) never appears in HTTP headers and is vanishingly rare in
// text bodies, so it's a safe field delimiter for the common REST/RPC payloads.
EM_JS(char*, sim_http_xhr, (const char* method, const char* url,
                            const char* headers, const char* body), {
    var out;
    try {
        var xhr = new XMLHttpRequest();
        xhr.open(UTF8ToString(method) || "GET", UTF8ToString(url), false);  // false = synchronous
        var hs = UTF8ToString(headers);
        if (hs) hs.split("\n").forEach(function(line) {
            var i = line.indexOf(":");
            if (i > 0) {
                try { xhr.setRequestHeader(line.slice(0, i).trim(), line.slice(i + 1).trim()); }
                catch (e) {}
            }
        });
        var bs = UTF8ToString(body);
        xhr.send(bs && bs.length ? bs : null);
        out = String(xhr.status) + "\x1f" + (xhr.getAllResponseHeaders() || "") + "\x1f" + (xhr.responseText || "");
    } catch (e) {
        out = "0\x1f\x1f" + ((e && e.message) ? e.message : "xhr failed");
    }
    var len = lengthBytesUTF8(out) + 1;
    var p = _malloc(len);
    stringToUTF8(out, p, len);
    return p;
});

void WasmHttpClient::onRegister(Runtime&) {}

HttpResponse WasmHttpClient::request(const char* method, const char* url,
                                     const char* headers, const char* body, bool) {
    HttpResponse r;
    char* p = sim_http_xhr(method ? method : "GET", url ? url : "",
                           headers ? headers : "", body ? body : "");
    if (!p) return r;
    std::string s(p);
    free(p);
    size_t a = s.find('\x1f');
    size_t b = (a == std::string::npos) ? std::string::npos : s.find('\x1f', a + 1);
    if (a != std::string::npos && b != std::string::npos) {
        r.status  = atoi(s.substr(0, a).c_str());
        r.headers = s.substr(a + 1, b - a - 1);
        r.body    = s.substr(b + 1);
    }
    return r;
}

HttpResponse WasmHttpClient::get(const char* url, bool insecure) {
    return request("GET", url, nullptr, nullptr, insecure);
}

HttpResponse WasmHttpClient::post(const char* url, const char* body,
                                  const char* contentType, bool insecure) {
    std::string hdr;
    if (contentType && *contentType) { hdr = "Content-Type: "; hdr += contentType; }
    return request("POST", url, hdr.empty() ? nullptr : hdr.c_str(), body, insecure);
}

}  // namespace nema
