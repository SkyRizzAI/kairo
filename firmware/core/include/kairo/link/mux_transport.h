#pragma once
#include "kairo/link/transport.h"

namespace kairo {

// MuxTransport — combine several ILinkTransports into one logical cable so the
// device is reachable over whichever connects (BLE, USB-CDC, …) without changing
// LinkService/RemoteService (they still see ONE transport). send() goes to every
// connected child; incoming data from any child funnels out the single onRecv.
// (Plan 37 — lets USB remote coexist with BLE on one RemoteService/screen-tap.)
class MuxTransport : public ILinkTransport {
public:
    static constexpr int MAX = 4;

    void add(ILinkTransport* t) {
        if (!t || count_ >= MAX) return;
        children_[count_++] = t;
        t->onRecv(&MuxTransport::recvThunk, this);
    }

    bool send(const uint8_t* data, size_t len) override {
        bool any = false;
        for (int i = 0; i < count_; i++)
            if (children_[i]->isConnected()) any |= children_[i]->send(data, len);
        return any;
    }
    void onRecv(RecvFn fn, void* user) override { recv_ = fn; user_ = user; }
    bool isConnected() const override {
        for (int i = 0; i < count_; i++) if (children_[i]->isConnected()) return true;
        return false;
    }
    size_t mtu() const override {
        size_t m = 512;
        for (int i = 0; i < count_; i++) { size_t c = children_[i]->mtu(); if (c && c < m) m = c; }
        return m;
    }

private:
    static void recvThunk(void* user, const uint8_t* d, size_t n) {
        auto* self = static_cast<MuxTransport*>(user);
        if (self->recv_) self->recv_(self->user_, d, n);
    }
    ILinkTransport* children_[MAX] = {};
    int             count_ = 0;
    RecvFn          recv_  = nullptr;
    void*           user_  = nullptr;
};

} // namespace kairo
