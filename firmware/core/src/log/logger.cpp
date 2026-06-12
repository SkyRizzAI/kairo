#include "nema/log/logger.h"

namespace nema {

Logger::Logger(IClock& clock) : clock_(clock) {}

void Logger::addSink(ILogSink& sink) {
    sinks_.push_back(&sink);
}

void Logger::setMinLevel(LogLevel lvl) {
    minLevel_ = lvl;
}

void Logger::log(LogLevel lvl, const char* component, std::string msg,
                 std::vector<Field> fields) {
    if (static_cast<uint8_t>(lvl) < static_cast<uint8_t>(minLevel_)) return;

    std::lock_guard<std::mutex> lock(mutex_);
    LogEntry entry;
    entry.epochMs   = clock_.epochMs();
    entry.level     = lvl;
    entry.component = component;
    entry.message   = std::move(msg);
    entry.fields    = std::move(fields);

    for (auto* sink : sinks_) {
        sink->write(entry);
    }
}

} // namespace nema
