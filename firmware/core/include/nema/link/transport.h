#pragma once
#include <cstdint>
#include <cstddef>

// PLP transport abstraction (device side). The remote protocol (PLP) is always
// the same; only the transport — the "cable" — changes. BLE, USB-CDC, and the
// simulator virtual-cable all implement this. See Plan 35.
namespace nema {

struct ILinkTransport {
    virtual ~ILinkTransport() = default;
    virtual bool   send(const uint8_t* data, size_t len) = 0;  // one datagram (<= mtu)
    using RecvFn = void (*)(void* user, const uint8_t* data, size_t len);
    virtual void   onRecv(RecvFn fn, void* user) = 0;
    virtual bool   isConnected() const = 0;
    virtual size_t mtu() const = 0;
};

// In-process loopback — two transports wired back-to-back. Used in host tests
// and as the model for the virtual cable: whatever A sends, B receives.
class LoopbackTransport : public ILinkTransport {
public:
    void setPeer(LoopbackTransport* p) { peer_ = p; }

    bool send(const uint8_t* data, size_t len) override {
        if (peer_ && peer_->recv_) peer_->recv_(peer_->user_, data, len);
        return true;
    }
    void onRecv(RecvFn fn, void* user) override { recv_ = fn; user_ = user; }
    bool isConnected() const override { return peer_ != nullptr; }
    size_t mtu() const override { return 512; }

private:
    LoopbackTransport* peer_ = nullptr;
    RecvFn             recv_ = nullptr;
    void*              user_ = nullptr;
};

} // namespace nema
