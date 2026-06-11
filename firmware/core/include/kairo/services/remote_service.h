#pragma once
#include "kairo/link/link_service.h"
#include "kairo/log/log_sink.h"
#include "kairo/system/board_profile.h"
#include <cstdint>
#include <string>

// RemoteService — device-side orchestrator for the KLP remote layer (Plan 35).
// Routes inbound channels (INPUT → InputService, SYSTEM → power callback) and
// streams logs out on the LOG channel via an ILogSink. The screen path is the
// separate RemoteScreenTap (registered as the IDisplayDriver). Transport-agnostic
// — works over BLE, USB, or the WASM virtual cable.
namespace kairo {

class InputService;
class Logger;
class EventBus;
class CliService;
struct IFileSystem;

// SYSTEM channel opcodes (first payload byte).
namespace SysOp {
    enum : uint8_t { GetInfo = 0x01, Restart = 0x10, Sleep = 0x11, Shutdown = 0x12 };
}

// FILE channel opcodes (first payload byte). Request host→device, reply
// device→host as [op][status][path\0][...]; status 0=ok, 1=not found, 2=error.
namespace FileOp {
    enum : uint8_t { List = 0x01, Read = 0x03, Write = 0x04, Mkdir = 0x05, Remove = 0x06 };
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
    void attachCli(CliService& cli) { cli_ = &cli; }   // route CLI channel here
    void attachFs(IFileSystem& fs) { fs_ = &fs; }      // route FILE channel here
    void onPower(PowerFn fn, void* user) { powerFn_ = fn; powerUser_ = user; }
    void onControl(ControlFn fn, void* user) { controlFn_ = fn; controlUser_ = user; }
    // Board profile (name, LCD, buttons) serialized to JSON and replied on
    // GetInfo as [SysOp::GetInfo][json] — the host matches its virtual board.
    void setProfile(const BoardProfile& p) { info_ = serializeBoardProfile(p); }

private:
    // LOG channel sink: serialize [level:1][component\0][message\0].
    class LinkLogSink : public ILogSink {
    public:
        LinkService* link = nullptr;
        void write(const LogEntry& e) override;
    };

    static void onFrameThunk(void* user, const klp::Frame& f);
    void dispatch(const klp::Frame& f);
    void handleFile(const std::vector<uint8_t>& in);   // FILE channel request → reply

    LinkService*  link_   = nullptr;
    InputService* input_  = nullptr;
    EventBus*     events_ = nullptr;
    CliService*   cli_    = nullptr;
    IFileSystem*  fs_     = nullptr;
    PowerFn       powerFn_ = nullptr;
    void*         powerUser_ = nullptr;
    ControlFn     controlFn_ = nullptr;
    void*         controlUser_ = nullptr;
    std::string   info_   = "{\"id\":\"kairo\",\"name\":\"kairo\",\"components\":[]}";
    LinkLogSink   logSink_;
};

} // namespace kairo
