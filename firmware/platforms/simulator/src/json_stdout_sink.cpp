#include "kairo/log/log_sink.h"
#include "kairo/log/log_entry.h"
#include "kairo/types.h"
#include "kairo/sim/bridge.h"
#include <nlohmann/json.hpp>

namespace kairo {

static const char* levelStr(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

// Defined here; registered into Logger by TelemetryBridge::init().
struct JsonStdoutSink : ILogSink {
    void write(const LogEntry& e) override {
        nlohmann::json j;
        j["type"]      = "log";
        j["ts"]        = e.epochMs;
        j["level"]     = levelStr(e.level);
        j["component"] = e.component;
        j["message"]   = e.message;
        if (!e.fields.empty()) {
            nlohmann::json fields;
            for (const auto& f : e.fields) fields[f.key] = f.value;
            j["fields"] = fields;
        }
        simEmit(j.dump());   // routed through one mutex — safe across threads
    }
};

// Factory used by TelemetryBridge
ILogSink* makeJsonStdoutSink() { return new JsonStdoutSink{}; }

} // namespace kairo
