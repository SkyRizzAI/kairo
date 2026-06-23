#pragma once
#include "nema/link/link_service.h"
#include "nema/log/log_sink.h"
#include "nema/system/board_profile.h"
#include "nema/services/cli_service.h"
#include "nema/services/remote_auth.h"
#include <cstdint>
#include <string>
#include <atomic>
#include <mutex>

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

// SYSTEM channel opcodes (first payload byte). Power ops are >= Restart (0x10);
// keep informational/control ops below that so the power gate in dispatch() stays
// a simple threshold check.
namespace SysOp {
    enum : uint8_t {
        GetInfo      = 0x01,
        ScreenStream = 0x02,   // [op][on:1] — opt in/out of the screen mirror (Plan 88)
        Restart      = 0x10, Sleep = 0x11, Shutdown = 0x12, Bootloader = 0x13,
    };
}

// FILE channel opcodes (first payload byte). FILE v2 (Plan 88): every request
// and reply carries a 2-byte little-endian reqId right after the opcode, so the
// host correlates replies by id (Map) instead of the fragile FIFO-per-opcode of
// v1. Wire layout:
//   request  host→device : [op][reqId:2][op-specific...]
//   reply    device→host : [op][reqId:2][status][extra...]
// status: 0=ok, 1=not found, 2=error, 7=bad request.
//
// Large writes use the chunked transfer (WriteBegin→WriteData×N→WriteEnd) so the
// host paces bytes against the device USB-CDC RX ring (a single whole-file frame
// overran it — the root cause of the `cp` failure). Each WriteData is acked with
// the device's next expected offset, making retries idempotent.
namespace FileOp {
    enum : uint8_t {
        List = 0x01, Read = 0x03, Mkdir = 0x05, Remove = 0x06, Rename = 0x07, Copy = 0x08,
        // Chunked write (FILE v2):
        WriteBegin = 0x10,   // [reqId][totalSize:4][path]      → ack [status][chunkSize:2]
        WriteData  = 0x11,   // [reqId][offset:4][bytes]        → ack [status][nextOffset:4]
        WriteEnd   = 0x12,   // [reqId]                          → ack [status]
        // Chunked read (device→host push; host RX tolerates bursts, so no per-chunk ack):
        ReadBegin  = 0x13,   // host→device [reqId][path]
                             //   device→host ack [reqId][status][totalSize:4][chunkSize:2]
                             //   then ReadData×N, then ReadEnd
        ReadData   = 0x14,   // device→host [reqId][offset:4][bytes]
        ReadEnd    = 0x15,   // device→host [reqId][status]
    };
}

// FileStatus — structured reply codes (Plan 88 §10/R6). Replaces the opaque "2".
namespace FileStatus {
    enum : uint8_t {
        Ok = 0, NotFound = 1, Error = 2,
        NoSpace = 3, TooManyOpenFiles = 4, PermissionDenied = 5, IoError = 6,
        BadRequest = 7,
    };
    // Map a POSIX errno to the closest FileStatus (best-effort, for write failures).
    inline uint8_t fromErrno(int e) {
        switch (e) {
            case 28 /*ENOSPC*/: return NoSpace;
            case 24 /*EMFILE*/: case 23 /*ENFILE*/: return TooManyOpenFiles;
            case 13 /*EACCES*/: case 30 /*EROFS*/:  return PermissionDenied;
            case 2  /*ENOENT*/: return NotFound;
            case 5  /*EIO*/:    return IoError;
            default:            return Error;
        }
    }
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
        AppInstall      = 0x03,   // payload = raw .papp bytes → JsAppStore (Plan 37)
        AppScan         = 0x04,   // rescan /system/apps/ and reload registry (Plan 86 Fase 6)
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
    void attachAuth(RemoteAuthStore& a) { auth_ = &a; }  // session auth policy (Plan 74)
    // Session-ready passthrough: RemoteService owns LinkService::onReady (to start
    // auth), so platforms register their screen-push here instead of on LinkService.
    void onReady(LinkService::ReadyFn fn, void* user) { readyFn_ = fn; readyUser_ = user; }
    void onPower(PowerFn fn, void* user) { powerFn_ = fn; powerUser_ = user; }
    void onControl(ControlFn fn, void* user) { controlFn_ = fn; controlUser_ = user; }

    // FILE-channel ops run inline on the receiving task (cdc_rx on ESP32, page-thread
    // on WASM). An earlier async-deferral seam (Plan 87/D5) was removed in Plan 88 —
    // at low priority its task was starved and silently dropped file ops; the chunked
    // write protocol makes inline handling cheap and reliable (ADR 0009).
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
    static void onReadyThunk(void* user);              // handshake done → start auth
    static bool gateThunk(void* user);                 // accept handshake? (Remote enabled)
    void startAuth();                                  // challenge or open (no pw)
    void handleAuthControl(const plp::Frame& f);       // process AUTH_* from host
    void sendCtrl(uint8_t op, const uint8_t* data, size_t len);
    void dispatch(const plp::Frame& f);
    void handleFile(const std::vector<uint8_t>& in);   // FILE channel request → reply
    void handleOta(const std::vector<uint8_t>& in);    // OTA channel: drive IOtaUpdater
    void sendCli(uint8_t sid, const std::string& text);   // frame: [sid][text]

    LinkService*  link_   = nullptr;
    InputService* input_  = nullptr;
    Logger*       log_    = nullptr;   // for file/transfer diagnostics
    EventBus*     events_ = nullptr;
    CliService*   cli_    = nullptr;
    IFileSystem*  fs_     = nullptr;
    IOtaUpdater*  ota_     = nullptr;
    RemoteAuthStore* auth_ = nullptr;            // Plan 74 — session auth policy
    // Auth state is read on the RX task (dispatch/handleAuthControl) but mutated on
    // a different thread when Remote is toggled off (RemoteToggled event →
    // onDisconnectThunk). authorized_ is atomic; nonce_ + sessions_ mutation is
    // serialised by stateMtx_ (Plan 88 R1).
    std::atomic<bool> authorized_{true};         // privileged allowed? (true = no-pw default)
    std::string   nonce_;                        // current challenge nonce
    std::mutex    stateMtx_;                      // guards nonce_ + sessions_ transitions
    LinkService::ReadyFn readyFn_ = nullptr;     // platform screen-push passthrough
    void*         readyUser_ = nullptr;
    PowerFn       powerFn_ = nullptr;
    void*         powerUser_ = nullptr;
    ControlFn     controlFn_ = nullptr;
    void*         controlUser_ = nullptr;
    // Active chunked-write transfer (FILE v2). At most one in flight. All FILE
    // ops run on a single handler context — the `file_rx` task on ESP32, or
    // inline on the WASM page-thread — so this needs no extra lock; it is never
    // touched from two threads at once.
    // A chunked write is one transfer identified by xferId (assigned by the device at
    // WriteBegin). Every WriteData/WriteEnd carries it so a second client sharing this
    // device link can't append into the wrong buffer (N2). The per-message reqId (in
    // the envelope) is separate — it only correlates each reply to its request.
    struct FileXfer {
        bool                 active    = false;
        bool                 streaming = false;  // true = write straight to disk (R5)
        uint16_t             xferId    = 0;
        uint32_t             written   = 0;      // bytes committed (streaming mode)
        std::string          path;
        std::vector<uint8_t> buf;     // RAM fallback only (backend lacks streaming, e.g. WASM)
    };
    FileXfer xfer_;
    uint16_t xferCounter_   = 0;      // assigns xferId (skips 0)
    // Idempotent WriteEnd (N3): remember the last completed transfer so a retried
    // WriteEnd (its ack was lost) replays the same status instead of BadRequest.
    uint16_t lastEndXferId_ = 0;      // 0 = none
    uint8_t  lastEndStatus_ = 0;
    std::string         info_   = "{\"id\":\"nema\",\"name\":\"nema\",\"components\":[]}";
    LinkLogSink         logSink_;
    CliSessionManager*  sessions_ = nullptr;   // multi-session shell state (Plan 45)
};

} // namespace nema
