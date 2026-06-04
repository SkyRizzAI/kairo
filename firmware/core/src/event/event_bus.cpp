#include "kairo/event/event_bus.h"
#include <algorithm>

namespace kairo {

SubscriptionId EventBus::subscribe(const char* name, EventHandler handler) {
    SubscriptionId id = nextId_++;
    subs_.push_back({id, std::string(name), std::move(handler)});
    return id;
}

void EventBus::unsubscribe(SubscriptionId id) {
    subs_.erase(
        std::remove_if(subs_.begin(), subs_.end(),
            [id](const Sub& s) { return s.id == id; }),
        subs_.end());
}

void EventBus::publish(const Event& event) {
    // Copy sub list so handlers can subscribe/unsubscribe safely during dispatch
    auto snapshot = subs_;
    publishing_ = true;
    for (const auto& sub : snapshot) {
        if (sub.name == "*" || sub.name == event.name) {
            sub.handler(event);
        }
    }
    publishing_ = false;
}

} // namespace kairo
