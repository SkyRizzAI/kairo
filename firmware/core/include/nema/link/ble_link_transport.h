#pragma once
#include "nema/link/transport.h"
#include "nema/link/klp_ble.h"
#include "nema/hal/bluetooth.h"
#include <cstring>

// BleLinkTransport — ILinkTransport over a BLE GATT peripheral (Plan 35).
//
// The "cable" is the KLP GATT service (see klp_ble.h): the device NOTIFYs frames
// on the TX characteristic and the host WRITEs frames to the RX characteristic.
// This wraps any IBleAdapter, so it is board-agnostic — Esp32Ble today, any
// future BLE radio tomorrow. The KLP protocol above it is identical to the BLE,
// USB-CDC, and WASM virtual-cable transports.
namespace nema {

class BleLinkTransport : public ILinkTransport {
public:
    // Registers the KLP GATT service on the adapter and routes RX writes to KLP.
    // Call once, before advertising starts.
    void init(IBleAdapter& ble) {
        ble_ = &ble;
        static const BleCharacteristic chars[] = {
            { klp_ble::CHAR_TX, BleProp::Notify },
            { klp_ble::CHAR_RX, BleProp::Write  },
        };
        static const BleService svc{ klp_ble::SERVICE, chars, 2 };
        ble_->registerService(svc);
        ble_->onWrite(&BleLinkTransport::writeThunk, this);
    }

    // ── ILinkTransport ──
    bool send(const uint8_t* data, size_t len) override {
        return ble_ && ble_->notify(klp_ble::CHAR_TX, data, len);
    }
    void onRecv(RecvFn fn, void* user) override { recv_ = fn; user_ = user; }
    bool isConnected() const override { return ble_ && ble_->isConnected(); }
    // BLE 4.2+ ATT MTU is negotiated; 180 stays safely under the common 185-byte
    // payload after the 3-byte ATT header on a 23..247 MTU link.
    size_t mtu() const override { return 180; }

private:
    static void writeThunk(void* user, const char* uuid, const uint8_t* d, size_t n) {
        auto* self = static_cast<BleLinkTransport*>(user);
        // Only the RX characteristic carries inbound KLP; ignore other writes.
        if (std::strcmp(uuid, klp_ble::CHAR_RX) != 0) return;
        if (self->recv_) self->recv_(self->user_, d, n);
    }

    IBleAdapter* ble_  = nullptr;
    RecvFn       recv_ = nullptr;
    void*        user_ = nullptr;
};

} // namespace nema
