#pragma once
#include "nema/log/log_entry.h"

namespace nema {

struct ILogSink {
    virtual ~ILogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
};

} // namespace nema
