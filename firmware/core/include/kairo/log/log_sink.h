#pragma once
#include "kairo/log/log_entry.h"

namespace kairo {

struct ILogSink {
    virtual ~ILogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
};

} // namespace kairo
