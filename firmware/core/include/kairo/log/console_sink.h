#pragma once
#include "kairo/log/log_sink.h"

namespace kairo {

// Writes human-readable log lines to stdout.
// Format: [HH:MM:SS] [LEVEL] [Component] message  key=value ...
// This is the only place in the codebase that may write to stdout in human mode.
class ConsoleSink : public ILogSink {
public:
    void write(const LogEntry& entry) override;
};

} // namespace kairo
