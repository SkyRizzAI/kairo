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
    klp::encodeFrame(buf, (uint8_t)klp::Channel::Control, &b, 1, 0);
    if (t_) t_->send(buf.data(), buf.size());
}

void LinkService::handle(const klp::Frame& f) {
    if (f.channel == (uint8_t)klp::Channel::Control) {
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

void LinkService::send(klp::Channel ch, const uint8_t* data, size_t len, uint8_t flags) {
    if (!t_) return;
    if (ch != klp::Channel::Control && !ready_.load()) return;   // gated until handshake
    std::vector<uint8_t> buf;   // local → thread-safe
    klp::encodeFrame(buf, (uint8_t)ch, data, len, flags);
    t_->send(buf.data(), buf.size());
}

} // namespace nema
