#pragma once
#include "nema/log/log_sink.h"
#include <deque>
#include <cstddef>

namespace nema {

// Ring buffer that retains the last N log entries for introspection.
// Oldest entry is evicted when capacity is reached.
class MemorySink : public ILogSink {
public:
    explicit MemorySink(std::size_t capacity);
    void write(const LogEntry& entry) override;
    const std::deque<LogEntry>& entries() const;
    void clear();

private:
    std::size_t         capacity_;
    std::deque<LogEntry> entries_;
};

} // namespace nema
