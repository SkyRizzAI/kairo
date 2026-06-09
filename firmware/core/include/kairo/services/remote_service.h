#pragma once
#include "kairo/link/link_service.h"
#include "kairo/log/log_sink.h"
#include <cstdint>

// RemoteService — device-side orchestrator for the KLP remote layer (Plan 35).
// Routes inbound channels (INPUT → InputService, SYSTEM → power callback) and
// streams logs out on the LOG channel via an ILogSink. The screen path is the
// separate RemoteScreenTap (registered as the IDisplayDriver). Transport-agnostic
// — works over BLE, USB, or the WASM virtual cable.
namespace kairo {

class InputService;
class Logger;
class EventBus;

// SYSTEM channel opcodes (first payload byte).
namespace SysOp {
    enum : uint8_t { GetInfo = 0x01, Restart = 0x10, Sleep = 0x11, Shutdown = 0x12 };
}

// EXT channel opcodes (host→device sim control).
namespace ExtOp {
    enum : uint8_t {
        InjectEvent     = 0x01,
        WifiSetNetworks = 0x02,
        AppInstall      = 0x03,   // payload = raw .kapp bytes → JsAppStore (Plan 37)
    };
}

class RemoteService {
public:
    using PowerFn = void (*)(void* user, uint8_t op);
    // Board/platform-specific EXT commands the core doesn't handle (e.g. wifi).
    using ControlFn = void (*)(void* user, uint8_t op, const uint8_t* data, size_t len);

    void init(LinkService& link, InputService& input);
    void attachLog(Logger& log);                 // install LOG-channel sink
    void attachEvents(EventBus& bus);            // stream EventBus → EVENT channel
    void onPower(PowerFn fn, void* user) { powerFn_ = fn; powerUser_ = user; }
    void onControl(ControlFn fn, void* user) { controlFn_ = fn; controlUser_ = user; }
    void setInfo(const char* info) { info_ = info; }   // replied on GetInfo

private:
    // LOG channel sink: serialize [level:1][component\0][message\0].
    class LinkLogSink : public ILogSink {
    public:
        LinkService* link = nullptr;
        void write(const LogEntry& e) override;
    };

    static void onFrameThunk(void* user, const klp::Frame& f);
    void dispatch(const klp::Frame& f);

    LinkService*  link_   = nullptr;
    InputService* input_  = nullptr;
    EventBus*     events_ = nullptr;
    PowerFn       powerFn_ = nullptr;
    void*         powerUser_ = nullptr;
    ControlFn     controlFn_ = nullptr;
    void*         controlUser_ = nullptr;
    const char*   info_   = "kairo";
    LinkLogSink   logSink_;
};

} // namespace kairo
