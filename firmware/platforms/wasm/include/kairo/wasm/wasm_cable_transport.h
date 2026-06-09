#pragma once
#include "kairo/link/transport.h"

namespace kairo {

// WasmCableTransport — the "virtual cable" (Plan 35). Carries KLP datagrams
// between the WASM firmware and the Forge host via postMessage. Outbound:
// send() posts a message to the worker host. Inbound: the exported C function
// kairo_klp_recv() (called from JS) feeds bytes back to onRecv().
class WasmCableTransport : public ILinkTransport {
public:
    void init();

    bool   send(const uint8_t* data, size_t len) override;
    void   onRecv(RecvFn fn, void* user) override { recv_ = fn; user_ = user; }
    bool   isConnected() const override { return true; }
    size_t mtu() const override { return 16384; }

    // Called by the exported C entry point when JS delivers inbound bytes.
    void deliver(const uint8_t* data, size_t len) { if (recv_) recv_(user_, data, len); }

private:
    RecvFn recv_ = nullptr;
    void*  user_ = nullptr;
};

} // namespace kairo
