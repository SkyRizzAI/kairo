#include "kairo/event/async_event_poster.h"
#include "kairo/event/event_bus.h"

namespace kairo {

void AsyncEventPoster::post(Event e) {
    queue_.send(std::move(e));
}

void AsyncEventPoster::flush(EventBus& bus) {
    // Drain everything queued since last frame. tryReceive holds the queue's
    // lock only for the pop, so publish() (which may post new events) can't
    // deadlock against it.
    Event e;
    while (queue_.tryReceive(e)) {
        bus.publish(e);
    }
}

} // namespace kairo
