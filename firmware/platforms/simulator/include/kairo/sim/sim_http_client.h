#pragma once
#include "kairo/hal/http_client.h"
#include "kairo/service.h"

namespace kairo {

class Logger;
class SimWifiDriver;

// Host HTTP client — shells out to `curl` so the simulator can hit real APIs
// (e.g. Binance) for an honest networked-app demo. Blocking; call from a worker.
//
// Honesty: curl uses the host Mac's network, which would let fetches succeed
// even when the simulated WiFi is "disconnected" — unlike real hardware. So we
// gate get() on the WiFi driver's isConnected(): no WiFi → transport error,
// exactly like the device.
class SimHttpClient : public IHttpClient, public IService {
public:
    void init(Logger& log, SimWifiDriver* wifi) { log_ = &log; wifi_ = wifi; }

    const char* name() const override { return "SimHttpClient"; }

    HttpResponse get(const char* url, bool insecure = true) override;

    void start() override {}
    void stop()  override {}

private:
    Logger*        log_  = nullptr;
    SimWifiDriver* wifi_ = nullptr;
};

} // namespace kairo
