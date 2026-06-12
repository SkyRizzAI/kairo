#include "nema/log/memory_sink.h"

namespace nema {

MemorySink::MemorySink(std::size_t capacity) : capacity_(capacity) {}

void MemorySink::write(const LogEntry& entry) {
    if (entries_.size() >= capacity_) {
        entries_.pop_front();
    }
    entries_.push_back(entry);
}

const std::deque<LogEntry>& MemorySink::entries() const {
    return entries_;
}

void MemorySink::clear() {
    entries_.clear();
}

} // namespace nema
