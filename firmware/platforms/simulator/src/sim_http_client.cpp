#include "kairo/sim/sim_http_client.h"
#include "kairo/sim/sim_wifi_driver.h"
#include "kairo/log/logger.h"
#include <array>
#include <cstdio>
#include <string>

namespace kairo {

// Runs `curl` and captures stdout. Blocking — invoked from a TaskRunner worker.
HttpResponse SimHttpClient::get(const char* url, bool insecure) {
    HttpResponse r;

    // Mirror real hardware: the internet only works when connected to a network
    // that is actually online (the web "router" panel can toggle a net offline).
    // curl would otherwise always use the host's real connection.
    if (wifi_ && !wifi_->isOnline()) {
        if (log_) log_->warn("SimHttpClient", "no internet (wifi offline/disconnected)");
        return r;   // status 0 = transport error
    }

    // Build command: -s silent, -w append status, optional -k insecure.
    // We append the HTTP code on its own final line via -w "\n%{http_code}".
    std::string cmd = "curl -s ";
    if (insecure) cmd += "-k ";
    cmd += "-m 10 ";                       // 10s timeout
    cmd += "-w '\\n%{http_code}' '";
    cmd += url;
    cmd += "' 2>/dev/null";

    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) { if (log_) log_->warn("SimHttpClient", "popen failed"); return r; }

    std::string out;
    std::array<char, 1024> buf;
    size_t n;
    while ((n = ::fread(buf.data(), 1, buf.size(), pipe)) > 0)
        out.append(buf.data(), n);
    ::pclose(pipe);

    // Split trailing status line.
    auto nl = out.find_last_of('\n');
    if (nl != std::string::npos) {
        r.body   = out.substr(0, nl);
        r.status = std::atoi(out.c_str() + nl + 1);
    } else {
        r.body = out;
    }
    if (log_) log_->info("SimHttpClient", "GET",
                         {{"status", std::to_string(r.status)},
                          {"bytes",  std::to_string(r.body.size())}});
    return r;
}

} // namespace kairo
