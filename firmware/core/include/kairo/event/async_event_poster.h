#pragma once
#include "kairo/event/event.h"
#include "kairo/nema/message_queue.h"

namespace kairo {

class EventBus;

// Thread-safe event poster for use from any thread/task.
//
// EventBus is single-threaded. Background threads (WiFi sys_evt, BLE, HTTP, OTA)
// must not call EventBus::publish() directly. Instead they post() here, and
// Runtime::step() drains via flush() in the main task.
//
// Now a thin wrapper over nema::MessageQueue<Event> — the cross-thread mechanism
// lives in the kernel, not duplicated here.
//
// Usage:
//   void MyDriver::onRegister(Runtime& rt) { poster_ = &rt.asyncPoster(); }
//   void MyDriver::onBackgroundCallback()  { poster_->post({events::Foo, {}}); }
class AsyncEventPoster {
public:
    void post(Event e);          // any thread — thread-safe, never drops (unbounded)
    void flush(EventBus& bus);   // main task only — drain into EventBus

private:
    nema::MessageQueue<Event> queue_{0};  // 0 = unbounded (events must not be lost)
};

} // namespace kairo
