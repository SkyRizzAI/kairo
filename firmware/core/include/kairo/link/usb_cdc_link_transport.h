#pragma once
#include "kairo/link/transport.h"
#include "kairo/hal/usb_cdc.h"

// UsbCdcLinkTransport — ILinkTransport over a USB CDC-ACM byte pipe (Plan 35).
//
// USB is point-to-point and physically secure (no pairing), so the "cable" is
// just the raw CDC stream: write() goes to the host, onData() comes from it. The
// KLP FrameParser above reassembles datagrams, so CDC packet boundaries don't
// matter. Same KLP protocol as BLE and the WASM virtual cable — only the cable
// differs.
namespace kairo {

class UsbCdcLinkTransport : public ILinkTransport {
public:
    void init(IUsbCdc& cdc) {
        cdc_ = &cdc;
        cdc_->onData(&UsbCdcLinkTransport::dataThunk, this);
    }

    // ── ILinkTransport ──
    bool send(const uint8_t* data, size_t len) override {
        return cdc_ && cdc_->write(data, len) == len;
    }
    void onRecv(RecvFn fn, void* user) override { recv_ = fn; user_ = user; }
    bool isConnected() const override { return cdc_ && cdc_->isOpen(); }
    size_t mtu() const override { return 512; }   // CDC-ACM bulk endpoint packet

private:
    static void dataThunk(void* user, const uint8_t* d, size_t n) {
        auto* self = static_cast<UsbCdcLinkTransport*>(user);
        if (self->recv_) self->recv_(self->user_, d, n);
    }

    IUsbCdc* cdc_  = nullptr;
    RecvFn   recv_ = nullptr;
    void*    user_ = nullptr;
};

} // namespace kairo
