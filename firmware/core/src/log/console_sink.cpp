#include "kairo/log/console_sink.h"
#include <cstdio>
#include <ctime>

namespace kairo {

static const char* levelLabel(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "?????";
}

void ConsoleSink::write(const LogEntry& entry) {
    // Format timestamp from epochMs
    time_t sec = static_cast<time_t>(entry.epochMs / 1000);
    struct tm* t = localtime(&sec);
    char timeBuf[9] = "??:??:??";
    if (t) strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", t);

    // [HH:MM:SS] [LEVEL] [Component] message
    std::fprintf(stdout, "[%s] [%s] [%s] %s",
        timeBuf,
        levelLabel(entry.level),
        entry.component,
        entry.message.c_str());

    // Append structured fields: key=value
    for (const auto& f : entry.fields) {
        std::fprintf(stdout, "  %s=%s", f.key, f.value.c_str());
    }

    std::fputc('\n', stdout);
    std::fflush(stdout);
}

} // namespace kairo
