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
    for (auto& f : parser_.push(d, n)) handle(f);
}

void LinkService::sendControl(uint8_t op) {
    uint8_t b = op;
    std::vector<uint8_t> buf;   // local → thread-safe (send may run on any thread)
    plp::encodeFrame(buf, (uint8_t)plp::Channel::Control, &b, 1, 0);
    if (t_) t_->send(buf.data(), buf.size());
}

void LinkService::handle(const plp::Frame& f) {
    if (f.channel == (uint8_t)plp::Channel::Control) {
        if (f.payload.empty()) return;
        switch (f.payload[0]) {
            case HELLO:                       // device side: accept + ACK
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
                break;
        }
        return;
    }
    // App channels only after handshake.
    if (ready_ && fn_) fn_(user_, f);
}

void LinkService::send(plp::Channel ch, const uint8_t* data, size_t len, uint8_t flags) {
    if (!t_) return;
    if (ch != plp::Channel::Control && !ready_.load()) return;   // gated until handshake
    std::vector<uint8_t> buf;
    plp::encodeFrame(buf, (uint8_t)ch, data, len, flags);
    // Serialize the actual transmit: the GUI thread (screen-tap), the RX task
    // (OTA/CLI acks) and app threads (events) all send concurrently. Without this
    // two frames can interleave on the wire (a transport may write in MTU-sized
    // pieces) → corrupt frames → e.g. a lost OTA ack mid-upload.
    std::lock_guard<std::mutex> lk(sendMtx_);
    t_->send(buf.data(), buf.size());
}

} // namespace nema
