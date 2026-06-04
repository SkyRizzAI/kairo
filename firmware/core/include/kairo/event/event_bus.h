#pragma once
#include "kairo/event/event.h"
#include <functional>
#include <vector>
#include <string>
#include <cstdint>

namespace kairo {

using EventHandler     = std::function<void(const Event&)>;
using SubscriptionId   = uint32_t;

class EventBus {
public:
    // Subscribe to a specific event name, or "*" for all events.
    SubscriptionId subscribe(const char* name, EventHandler handler);
    void unsubscribe(SubscriptionId id);
    void publish(const Event& event);

private:
    struct Sub {
        SubscriptionId id;
        std::string    name;    // "*" = wildcard
        EventHandler   handler;
    };

    std::vector<Sub> subs_;
    SubscriptionId   nextId_ = 1;
    bool             publishing_ = false; // re-entrancy guard: copy subs before dispatch
};

} // namespace kairo
