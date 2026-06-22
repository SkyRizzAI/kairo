#include "nema/services/system_wifi_manager.h"
#include "nema/runtime.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/service/service_container.h"
#include "nema/services/resource_broker.h"
#include "nema/hal/wifi.h"
#include <string>

namespace nema {

void SystemWifiManager::init(Runtime& rt) {
    rt_ = &rt;

    // Acquire managed lease when device connects to a network.
    rt.events().subscribe(events::NetworkConnected, [this](const Event&) {
        auto* broker = rt_->container().resolve<ResourceBroker>();
        if (!broker) return;
        auto r = broker->acquire("system:wifi", "net.wifi.managed");
        if (r.ok) leaseHandle_ = r.value;
    });

    // Release managed lease on disconnect (user-initiated or signal lost).
    rt.events().subscribe(events::NetworkDisconnected, [this](const Event&) {
        if (!leaseHandle_) return;
        auto* broker = rt_->container().resolve<ResourceBroker>();
        if (broker) broker->release("system:wifi", leaseHandle_);
        leaseHandle_ = 0;
    });

    // An app took the WiFi radio — broker already released our system:wifi
    // lease. Tell the WiFi driver to disconnect so the radio is truly free.
    rt.events().subscribe(events::ResourceSuspended, [this](const Event& e) {
        std::string cap;
        for (const auto& f : e.payload)
            if (std::string(f.key) == "cap") { cap = f.value; break; }
        if (cap != "net.wifi.managed") return;
        leaseHandle_ = 0;  // broker already freed our lease
        auto* wifi = rt_->container().resolve<IWifiDriver>();
        if (wifi) wifi->disconnect();
    });

    // Exclusive app released the radio — reclaim the managed lease and reconnect.
    rt.events().subscribe(events::ResourceRestored, [this](const Event& e) {
        std::string cap;
        for (const auto& f : e.payload)
            if (std::string(f.key) == "cap") { cap = f.value; break; }
        if (cap != "net.wifi.managed") return;
        auto* broker = rt_->container().resolve<ResourceBroker>();
        auto* wifi   = rt_->container().resolve<IWifiDriver>();
        if (!broker || !wifi) return;
        auto r = broker->acquire("system:wifi", "net.wifi.managed");
        if (!r.ok) return;
        leaseHandle_ = r.value;
        // autoConnect() blocks — run on task worker, not the event thread.
        rt_->tasks().submit([wifi] { wifi->autoConnect(); });
    });
}

} // namespace nema
