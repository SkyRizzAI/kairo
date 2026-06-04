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
namespace kairo::nema {

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

    // Block up to timeoutMs for an item. Returns false on timeout.
    bool receive(T& out, uint32_t timeoutMs) {
        std::unique_lock<std::mutex> lk(m_);
        if (!cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                          [this] { return !q_.empty(); }))
            return false;
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

} // namespace kairo::nema
