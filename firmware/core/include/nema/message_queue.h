#pragma once
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <utility>

// Nema kernel — thread-safe message queue.
// Portable across ESP32 (libstdc++/pthread) and host. std::mutex is already
// proven to work on ESP-IDF (Logger uses it); std::condition_variable is
// pthread-backed there too.
//
// Not ISR-safe (condition_variable cannot be signalled from an ISR). For the
// input path we poll from a dedicated thread, not an ISR — see InputService.
namespace nema {

template <typename T>
class MessageQueue {
public:
    // capacity == 0 → unbounded (never drops). >0 → send() returns false when full.
    explicit MessageQueue(size_t capacity = 0) : capacity_(capacity) {}

    // Thread-safe. Returns false if bounded and full (item dropped).
    bool send(T item) {
        {
            std::lock_guard<std::mutex> lk(m_);
            if (capacity_ != 0 && q_.size() >= capacity_) return false;
            q_.push_back(std::move(item));
        }
        cv_.notify_one();
        return true;
    }

    // Sentinel: receive(out, FOREVER) blocks indefinitely (no timeout).
    static constexpr uint32_t FOREVER = 0xFFFFFFFFu;

    // Block up to timeoutMs for an item. Returns false on timeout. FOREVER blocks.
    bool receive(T& out, uint32_t timeoutMs) {
        std::unique_lock<std::mutex> lk(m_);
        if (timeoutMs == FOREVER) {
            // Block forever via cv.wait() — NOT wait_for(milliseconds(0xFFFFFFFF)).
            // A ~49-day millisecond count overflows the 32-bit tick arithmetic in the
            // ESP-IDF/libstdc++ timed-wait path, so wait_for() returns IMMEDIATELY
            // instead of blocking. Callers that loop on the result (wasm input_wait,
            // app event loops) then busy-spin: on hardware that starves the task
            // watchdog (canvas-demo "freeze"); in the WASM simulator the spinning
            // worker floods the main thread and hangs the page. cv.wait() does no
            // deadline math, so it truly parks the thread at ~0 CPU.
            cv_.wait(lk, [this] { return !q_.empty(); });
        } else if (!cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                                 [this] { return !q_.empty(); })) {
            return false;
        }
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }

    // Non-blocking pop. Returns false if empty.
    bool tryReceive(T& out) {
        std::lock_guard<std::mutex> lk(m_);
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }

private:
    mutable std::mutex      m_;
    std::condition_variable cv_;
    std::deque<T>           q_;
    size_t                  capacity_;
};

} // namespace nema
