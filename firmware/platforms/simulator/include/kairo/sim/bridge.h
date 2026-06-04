#pragma once
#include <string>

namespace kairo {

class Runtime;
struct ILogSink;

// Bridges EventBus events and service state transitions to JSON stdout.
class TelemetryBridge {
public:
    void init(Runtime& rt);
    void sendSnapshots();
private:
    Runtime* rt_ = nullptr;
};

// Reads JSON-lines commands from stdin (non-blocking) and dispatches to Runtime.
class CommandReader {
public:
    void init(Runtime& rt);
    void poll();  // call each loop iteration from SimulatorPlatform::idle()
private:
    Runtime*    rt_ = nullptr;
    std::string lineBuf_;
    void dispatch(const std::string& line);
};

// Thread-safe stdout emit. Frames are written from the GuiService thread while
// logs/events come from the main thread — this serialises whole JSON lines so
// the JSON-lines protocol never interleaves. All stdout writers route here.
void simEmit(const std::string& jsonLine);

// Factory: returns a new JsonStdoutSink (defined in json_stdout_sink.cpp)
ILogSink* makeJsonStdoutSink();

} // namespace kairo
