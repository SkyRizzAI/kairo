#pragma once
#include "nema/log/log_sink.h"
#include "nema/clock.h"
#include <initializer_list>
#include <string>
#include <vector>
#include <mutex>

namespace nema {

class Logger {
public:
    explicit Logger(IClock& clock);

    void addSink(ILogSink& sink);
    void setMinLevel(LogLevel lvl);

    void log(LogLevel lvl, const char* component, std::string msg,
             std::vector<Field> fields = {});

    void trace(const char* c, std::string m, std::vector<Field> f = {}) { log(LogLevel::Trace, c, std::move(m), std::move(f)); }
    void debug(const char* c, std::string m, std::vector<Field> f = {}) { log(LogLevel::Debug, c, std::move(m), std::move(f)); }
    void info (const char* c, std::string m, std::vector<Field> f = {}) { log(LogLevel::Info,  c, std::move(m), std::move(f)); }
    void warn (const char* c, std::string m, std::vector<Field> f = {}) { log(LogLevel::Warn,  c, std::move(m), std::move(f)); }
    void error(const char* c, std::string m, std::vector<Field> f = {}) { log(LogLevel::Error, c, std::move(m), std::move(f)); }
    void fatal(const char* c, std::string m, std::vector<Field> f = {}) { log(LogLevel::Fatal, c, std::move(m), std::move(f)); }

private:
    IClock&                clock_;
    LogLevel               minLevel_ = LogLevel::Trace;
    std::vector<ILogSink*> sinks_;
    std::mutex             mutex_;  // guards log() — safe to call from any task
};

} // namespace nema
