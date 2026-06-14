#pragma once
#include "nema/link/link_service.h"
#include "nema/log/log_sink.h"
#include "nema/system/board_profile.h"
#include "nema/services/cli_service.h"
#include <cstdint>
#include <string>

// RemoteService — device-side orchestrator for the PLP remote layer (Plan 35).
// Routes inbound channels (INPUT → InputService, SYSTEM → power callback) and
// streams logs out on the LOG channel via an ILogSink. The screen path is the
// separate RemoteScreenTap (registered as the IDisplayDriver). Transport-agnostic
// — works over BLE, USB, or the WASM virtual cable.
namespace nema {

class InputService;
class Logger;
class EventBus;
class CliService;
struct IFileSystem;
struct IOtaUpdater;

// SYSTEM channel opcodes (first payload byte).
namespace SysOp {
    enum : uint8_t { GetInfo = 0x01, Restart = 0x10, Sleep = 0x11, Shutdown = 0x12 };
}

// FILE channel opcodes (first payload byte). Request host→device, reply
// device→host as [op][status][path\0][...]; status 0=ok, 1=not found, 2=error.
namespace FileOp {
    enum : uint8_t { List = 0x01, Read = 0x03, Write = 0x04, Mkdir = 0x05, Remove = 0x06 };
}

// OTA channel opcodes (host→device firmware push, Plan 39). Reply on the same
// channel: [op][status]([written:4 LE]). status: 0=ok, 1=error, 2=unsupported.
// Data is [op][offset:4 LE][bytes] (idempotent retry by offset). The Begin reply
// also carries OtaProtoVersion so the host can detect a stale firmware and tell
// the user to re-flash, instead of failing mysteriously mid-upload.
namespace OtaOp {
    enum : uint8_t { Begin = 0x01, Data = 0x02, End = 0x03, Abort = 0x04 };
}
inline constexpr uint8_t OtaProtoVersion = 2;   // bump when the OTA wire format changes
namespace OtaStatus {
    enum : uint8_t { Ok = 0, Error = 1, Unsupported = 2 };
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
    void attachSessions(CliSessionManager& m) { sessions_ = &m; }  // multi-session (Plan 45)
    void attachFs(IFileSystem& fs) { fs_ = &fs; }      // route FILE channel here
    void attachOta(IOtaUpdater& ota) { ota_ = &ota; }  // route OTA channel here (Plan 39)
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

    static void onFrameThunk(void* user, const plp::Frame& f);
    static void onDisconnectThunk(void* user);         // drop all shell sessions
    void dispatch(const plp::Frame& f);
    void handleFile(const std::vector<uint8_t>& in);   // FILE channel request → reply
    void handleOta(const std::vector<uint8_t>& in);    // OTA channel: drive IOtaUpdater
    void sendCli(uint8_t sid, const std::string& text);   // frame: [sid][text]

    LinkService*  link_   = nullptr;
    InputService* input_  = nullptr;
    EventBus*     events_ = nullptr;
    CliService*   cli_    = nullptr;
    IFileSystem*  fs_     = nullptr;
    IOtaUpdater*  ota_     = nullptr;
    PowerFn       powerFn_ = nullptr;
    void*         powerUser_ = nullptr;
    ControlFn     controlFn_ = nullptr;
    void*         controlUser_ = nullptr;
    std::string         info_   = "{\"id\":\"nema\",\"name\":\"nema\",\"components\":[]}";
    LinkLogSink         logSink_;
    CliSessionManager*  sessions_ = nullptr;   // multi-session shell state (Plan 45)
};

} // namespace nema
