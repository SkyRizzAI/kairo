#pragma once
#include "nema/link/plp_codec.h"
#include "nema/link/transport.h"
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <vector>

// LinkService — PLP session over one ILinkTransport (Plan 35). Owns the codec,
// runs the handshake, and routes decoded frames to a handler. App channels
// (Screen/Input/Log/System/Ota/Ext) are gated until the handshake completes;
// Control is always allowed.
namespace nema {

class LinkService {
public:
    enum class Role : uint8_t { Device, Host };

    // Control opcodes (first payload byte on Channel::Control).
    enum : uint8_t {
        HELLO = 0x01, ACK = 0x02, REJECT = 0x03, PING = 0x10, PONG = 0x11,
        // Session auth (Plan 74) — handled by RemoteService (it owns the store).
        AUTH_CHALLENGE = 0x20,   // dev->host  "salt:nonce"
        AUTH_RESPONSE  = 0x21,   // host->dev  [kind 'H'|'T'][hexvalue]
        AUTH_OK        = 0x22,   // dev->host  [token]
        AUTH_FAIL      = 0x23,   // dev->host
        AUTH_REQUIRED  = 0x24,   // dev->host  (privileged attempted unauthorized)
    };

    using FrameFn      = void (*)(void* user, const plp::Frame& f);
    using ReadyFn      = void (*)(void* user);
    using DisconnectFn = void (*)(void* user);
    using GateFn       = bool (*)(void* user);   // may this session even start?

    void attach(ILinkTransport* t, Role role);
    void onFrame(FrameFn fn, void* user) { fn_ = fn; user_ = user; }
    // Plan 74: outbound non-Control channels (incl. the screen-tap) are dropped
    // until authorized; a password-protected device shows nothing pre-auth.
    void setAuthorized(bool a) { authorized_ = a; }
    bool authorized() const { return authorized_.load(); }
    // Plan 88: the screen mirror is opt-in per session. It is a heavy continuous
    // stream that, left always-on, starves the inbound file/CLI path on the
    // single USB RX task (a file-transfer CLI never wants it). Default OFF; the
    // host enables it explicitly (SysOp::ScreenStream). Reset on each new HELLO.
    void setScreenWanted(bool w) { screenWanted_ = w; }
    bool screenWanted() const { return screenWanted_.load(); }
    // Plan 74: device refuses the handshake (sends REJECT) when this returns false
    // — e.g. Remote disabled. Checked on HELLO before going ready.
    void onHandshakeGate(GateFn fn, void* user) { gate_ = fn; gateUser_ = user; }
    // Fired when the handshake completes (host connected) — e.g. to push the
    // current screen frame so a freshly-attached viewer isn't blank.
    void onReady(ReadyFn fn, void* user) { readyFn_ = fn; readyUser_ = user; }
    // Fired when the session drops (the owner's transport detected a disconnect
    // and called markDisconnected). The remote screen/input is gone after this.
    void onDisconnect(DisconnectFn fn, void* user) { disconnectFn_ = fn; disconnectUser_ = user; }
    // Called by the transport owner when the underlying link drops. Idempotent:
    // only transitions + fires the callback if the session was previously ready.
    // NOTE: fires on the caller's thread (typically a transport RX/event task) —
    // handlers must be thread-safe and must NOT call CapabilityRegistry::setState
    // directly (route liveness through the async event path instead).
    void markDisconnected();

    // Host initiates the handshake (sends HELLO). Device replies ACK on receipt.
    void begin();
    bool ready() const { return ready_.load(); }

    // In-protocol liveness (Plan 88 §8): USB/WASM give no disconnect signal, so the
    // host PINGs periodically and the device watches for silence. Call livenessTick()
    // on a periodic timer. Only declares the peer dead after MANY empty windows — a
    // GENEROUS grace, because file ops run inline on the RX task and a slow SD/LittleFS
    // flush legitimately stops onBytes() for seconds; a tight timeout false-positived
    // mid-transfer and killed large writes. ~20 ticks × 3 s ≈ 60 s.
    void livenessTick() {
        if (!ready_.load()) return;
        if (rxSeen_.exchange(false)) { missedTicks_ = 0; return; }  // traffic → alive
        if (missedTicks_.fetch_add(1) + 1 >= 20)                    // ~60 s silence → dead
            markDisconnected();
    }

    // Send a frame. Non-control channels are dropped until ready().
    void send(plp::Channel ch, const uint8_t* data, size_t len, uint8_t flags = 0);

private:
    static void recvThunk(void* user, const uint8_t* d, size_t n);
    void onBytes(const uint8_t* d, size_t n);
    void handle(const plp::Frame& f);
    void sendControl(uint8_t op);

    ILinkTransport*   t_     = nullptr;
    Role              role_  = Role::Device;
    std::atomic<bool> ready_{false};
    std::atomic<bool> authorized_{true};   // Plan 74 — gates outbound app channels
    std::atomic<bool> screenWanted_{false}; // Plan 88 — screen mirror opt-in per session
    std::atomic<bool> rxSeen_{false};       // Plan 88 — frame seen since last livenessTick
    std::atomic<int>  missedTicks_{0};      // consecutive empty liveness windows
    GateFn            gate_     = nullptr;
    void*             gateUser_ = nullptr;
    plp::FrameParser  parser_;
    FrameFn           fn_    = nullptr;
    void*             user_  = nullptr;
    ReadyFn           readyFn_   = nullptr;
    void*             readyUser_ = nullptr;
    DisconnectFn      disconnectFn_   = nullptr;
    void*             disconnectUser_ = nullptr;
    // (Transmit serialization lives inside the transport now — see esp32_usb_cdc — so
    // the WASM sim's GUI worker never blocks on the main thread while holding a lock.)
    std::mutex        recvMtx_;        // serialize concurrent onBytes() — MuxTransport
                                       // routes USB and WS bytes from different tasks
                                       // into the same FrameParser; without this lock
                                       // their bytes interleave → corrupt frames.
};

} // namespace nema
