#pragma once
#include "nema/link/klp_codec.h"
#include "nema/link/transport.h"
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>

// LinkService — KLP session over one ILinkTransport (Plan 35). Owns the codec,
// runs the handshake, and routes decoded frames to a handler. App channels
// (Screen/Input/Log/System/Ota/Ext) are gated until the handshake completes;
// Control is always allowed.
namespace nema {

class LinkService {
public:
    enum class Role : uint8_t { Device, Host };

    // Control opcodes (first payload byte on Channel::Control).
    enum : uint8_t { HELLO = 0x01, ACK = 0x02, REJECT = 0x03, PING = 0x10, PONG = 0x11 };

    using FrameFn      = void (*)(void* user, const klp::Frame& f);
    using ReadyFn      = void (*)(void* user);
    using DisconnectFn = void (*)(void* user);

    void attach(ILinkTransport* t, Role role);
    void onFrame(FrameFn fn, void* user) { fn_ = fn; user_ = user; }
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

    // Send a frame. Non-control channels are dropped until ready().
    void send(klp::Channel ch, const uint8_t* data, size_t len, uint8_t flags = 0);

private:
    static void recvThunk(void* user, const uint8_t* d, size_t n);
    void onBytes(const uint8_t* d, size_t n);
    void handle(const klp::Frame& f);
    void sendControl(uint8_t op);

    ILinkTransport*   t_     = nullptr;
    Role              role_  = Role::Device;
    std::atomic<bool> ready_{false};
    klp::FrameParser  parser_;
    FrameFn           fn_    = nullptr;
    void*             user_  = nullptr;
    ReadyFn           readyFn_   = nullptr;
    void*             readyUser_ = nullptr;
    DisconnectFn      disconnectFn_   = nullptr;
    void*             disconnectUser_ = nullptr;
};

} // namespace nema
