#include "nema/log/file_sink.h"
#include "nema/log/log_entry.h"
#include <cstdio>
#include <ctime>

namespace nema {

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

FileSink::FileSink(std::string path) : path_(std::move(path)) {}

FileSink::~FileSink() {
    if (file_) { fflush(file_); fclose(file_); file_ = nullptr; }
}

void FileSink::write(const LogEntry& entry) {
    if (!file_) {
        if (retryIn_ > 0) { retryIn_--; return; }
        file_ = fopen(path_.c_str(), "a");
        if (!file_) { retryIn_ = kRetryInterval; return; }
    }

    time_t sec = static_cast<time_t>(entry.epochMs / 1000);
    struct tm* t = localtime(&sec);
    char timeBuf[9] = "??:??:??";
    if (t) strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", t);

    fprintf(file_, "[%s] [%s] [%s] %s",
            timeBuf, levelLabel(entry.level),
            entry.component, entry.message.c_str());
    for (const auto& f : entry.fields)
        fprintf(file_, "  %s=%s", f.key, f.value.c_str());
    fputc('\n', file_);
    fflush(file_);  // flush every line: survives unexpected resets
}

} // namespace nema
