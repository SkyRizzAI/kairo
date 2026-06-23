#include "nema/link/link_service.h"

namespace nema {

void LinkService::attach(ILinkTransport* t, Role role) {
    markDisconnected();          // tear down any prior session cleanly first
    t_ = t;
    role_ = role;
    ready_ = false;
    parser_.reset();
    if (t_) t_->onRecv(&LinkService::recvThunk, this);
}

void LinkService::markDisconnected() {
    if (!ready_.exchange(false)) return;   // was not ready → nothing to tear down
    screenWanted_ = false;                 // a new session must re-opt-in to the mirror
    {
        std::lock_guard<std::mutex> lk(recvMtx_);
        parser_.reset();                   // discard stale bytes so the next session starts clean
    }
    if (disconnectFn_) disconnectFn_(disconnectUser_);
}

void LinkService::begin() {
    // Host kicks off the handshake.
    if (role_ == Role::Host) sendControl(HELLO);
}

void LinkService::recvThunk(void* user, const uint8_t* d, size_t n) {
    static_cast<LinkService*>(user)->onBytes(d, n);
}

void LinkService::onBytes(const uint8_t* d, size_t n) {
    rxSeen_ = true;   // liveness: any inbound traffic marks the peer alive this window
    // Hold recvMtx_ only while the parser accumulates bytes into frames.
    // Releasing before handle() means a blocking file/OTA operation (D5) cannot
    // stall incoming HELLO frames from a reconnecting host, and MuxTransport's
    // USB + BLE tasks can interleave without starving each other.
    std::vector<plp::Frame> frames;
    {
        std::lock_guard<std::mutex> lk(recvMtx_);
        frames = parser_.push(d, n);
    }
    for (auto& f : frames) handle(f);
}

void LinkService::sendControl(uint8_t op) {
    if (!t_) return;
    uint8_t b = op;
    std::vector<uint8_t> buf;
    plp::encodeFrame(buf, (uint8_t)plp::Channel::Control, &b, 1, 0);
    // Whole-frame transmit. Concurrent senders (GUI screen-tap, RX task acks, app
    // threads) must not interleave bytes on the wire — that serialization now lives
    // INSIDE the transport (e.g. the HWCDC writer holds a mutex), NOT here. A lock
    // here deadlocked the WASM simulator: send() runs on the GUI worker and blocks on
    // the main thread (MAIN_THREAD_EM_ASM), while the main thread blocked acquiring
    // the same lock to send a handshake reply.
    t_->send(buf.data(), buf.size());
}

void LinkService::handle(const plp::Frame& f) {
    if (f.channel == (uint8_t)plp::Channel::Control) {
        if (f.payload.empty()) return;
        switch (f.payload[0]) {
            case HELLO:                       // device side: accept + ACK
                // Reset the parser first so stale bytes from a previous session
                // (USB has no disconnect notification) don't corrupt this one. Under
                // recvMtx_ — a MuxTransport feeds bytes from two tasks into one parser,
                // so clearing buf_ must not race a concurrent push() (W2).
                { std::lock_guard<std::mutex> lk(recvMtx_); parser_.reset(); }
                // A fresh session starts with the screen mirror OFF — the host
                // opts in explicitly (SysOp::ScreenStream). Keeps a file/CLI-only
                // session from drowning the RX path in screen frames.
                screenWanted_ = false;
                rxSeen_ = true; missedTicks_ = 0;   // reset liveness for the new session
                if (gate_ && !gate_(gateUser_)) {   // Remote disabled → refuse
                    sendControl(REJECT);
                    return;
                }
                ready_ = true;
                sendControl(ACK);
                if (readyFn_) readyFn_(readyUser_);
                break;
            case ACK:                         // host side: handshake complete
                ready_ = true;
                if (readyFn_) readyFn_(readyUser_);
                break;
            case PING:
                sendControl(PONG);
                break;
            default:
                // Auth + other Control opcodes are handled by the upper layer
                // (RemoteService owns the auth store). Forward once ready.
                if (ready_ && fn_) fn_(user_, f);
                break;
        }
        return;
    }
    // App channels only after handshake.
    if (ready_ && fn_) fn_(user_, f);
}

void LinkService::send(plp::Channel ch, const uint8_t* data, size_t len, uint8_t flags) {
    if (!t_) return;
    // Gated until handshake AND (Plan 74) until the session is authorized — this is
    // what blanks the screen-tap on a password-protected device before auth.
    if (ch != plp::Channel::Control && (!ready_.load() || !authorized_.load())) return;
    std::vector<uint8_t> buf;
    plp::encodeFrame(buf, (uint8_t)ch, data, len, flags);
    // Whole-frame transmit. Serialization against concurrent senders (GUI screen-tap,
    // RX task OTA/CLI acks, app event threads) lives INSIDE the transport now — the
    // HWCDC writer takes a mutex so two frames can't interleave on the wire. It must
    // NOT be a LinkService-level lock: send() runs on the GUI worker in the WASM sim
    // and blocks on the main thread (MAIN_THREAD_EM_ASM); a shared lock there deadlocks
    // against the main thread sending a handshake reply (Plan 88 — sim freeze fix).
    t_->send(buf.data(), buf.size());
}

} // namespace nema
