#pragma once
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdint>

// Nema kernel — single-consumer wake primitive (Plan 97).
//
// Turns the GUI loop from a fixed 30 fps poll into an event-driven loop: one
// thread (the GUI thread) parks in wait() until either the frame budget elapses
// OR a cross-thread producer calls signal(). This removes the up-to-33 ms poll
// latency on both edges of the input→pixel path:
//   • input arriving mid-frame wakes the loop immediately (InputService::post)
//   • an app finishing a frame wakes the loop immediately (AppHost::present)
//
// Only CROSS-THREAD producers signal. GUI-thread-internal redraws (animation,
// status bar, same-frame navigation) must NOT signal — they already render in the
// current frame, and signalling per-frame would turn the loop into a busy spin
// (the frame budget would always return early). See Plan 97 §4.
//
// Portable: std::mutex + std::condition_variable are proven on ESP-IDF (Logger,
// MessageQueue use them) and on the host/WASM build (emscripten pthreads).
namespace nema {

class Waker {
public:
    // Wake the waiter. Coalescing: multiple signals before the next wait() count
    // as one. Thread-safe — call from any thread.
    void signal() {
        { std::lock_guard<std::mutex> lk(m_); flag_ = true; }
        cv_.notify_one();
    }

    // Block up to timeoutMs, or return early if signalled. Consumes the flag so a
    // signal that arrived *before* the wait (e.g. an app present() between this
    // frame's render and this wait) is not lost — the predicate observes it and
    // returns immediately. Returns true if woken by a signal, false on timeout.
    bool wait(uint32_t timeoutMs) {
        std::unique_lock<std::mutex> lk(m_);
        bool woken = cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                                  [this] { return flag_; });
        flag_ = false;   // consume
        return woken;
    }

private:
    std::mutex              m_;
    std::condition_variable cv_;
    bool                    flag_ = false;
};

} // namespace nema
