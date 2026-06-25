#pragma once
#include "nema/link/transport.h"
#include "nema/link/plp_ble.h"
#include "nema/hal/bluetooth.h"
#include <cstring>

// BleLinkTransport — ILinkTransport over a BLE GATT peripheral (Plan 35).
//
// The "cable" is the PLP GATT service (see plp_ble.h): the device NOTIFYs frames
// on the TX characteristic and the host WRITEs frames to the RX characteristic.
// This wraps any IBleAdapter, so it is board-agnostic — Esp32Ble today, any
// future BLE radio tomorrow. The PLP protocol above it is identical to the BLE,
// USB-CDC, and WASM virtual-cable transports.
namespace nema {

class BleLinkTransport : public ILinkTransport {
public:
    // Registers the PLP GATT service on the adapter and routes RX writes to PLP.
    // Call once, before advertising starts.
    void init(IBleAdapter& ble) {
        ble_ = &ble;
        static const BleCharacteristic chars[] = {
            { plp_ble::CHAR_TX, BleProp::Notify },
            { plp_ble::CHAR_RX, BleProp::Write  },
        };
        static const BleService svc{ plp_ble::SERVICE, chars, 2 };
        ble_->registerService(svc);
        ble_->onWrite(&BleLinkTransport::writeThunk, this);
    }

    // ── ILinkTransport ──
    bool send(const uint8_t* data, size_t len) override {
        if (!ble_) return false;
        if (len == 0) return true;
        const size_t chunk = mtu();
        // ── "Smart-data" channel arbitration (Plan 93) ──────────────────────────────
        // BLE is low-bandwidth, so the Screen mirror (PLP channel 0x01, at data[1]) is
        // strictly BEST-EFFORT and YIELDS to the request/response channels (CLI, control,
        // file). It is sent ONLY when:
        //   (a) the link is nearly idle — txPending() ≤ kIdle, i.e. no CLI/control reply is
        //       still in flight. While a command runs, its reply keeps pending > kIdle so the
        //       mirror pauses and the CLI gets the whole link; when the reply drains, the
        //       mirror resumes. (Exactly the "stop the display while a command replies" idea.)
        //   (b) the frame is small enough that its burst of chunks can't overrun the
        //       controller's ACL buffers ("BLE_INIT: Malloc failed"). Big/busy frames are
        //       skipped; the next small one gets through.
        // Inbound input (host→device) is RX, not gated here, so it's always instant. CLI/
        // control replies are non-screen channels → never gated → always sent. The peak
        // in-flight (kIdle + kMaxChunks) stays well under the controller's buffer count.
        if (len >= 2 && data[1] == 0x01 /*plp::Channel::Screen*/) {
            // Send a screen frame ONLY when the host mbuf pool has room for ALL its chunks
            // right now — so it never goes out partial (the host would CRC-drop a truncated
            // frame → that's what froze the mirror after the first frame) and never hogs the
            // last slots needed by CLI/control. txPending() reads the live pool, so it self-
            // recovers as the radio drains; no manual counter to latch. This also yields to
            // CLI automatically: while a reply is in flight the pool is fuller, so the mirror
            // pauses, then resumes. (NOTE: only succeeds with WiFi OFF — with WiFi on the BLE
            // controller can't allocate TX buffers at all; radio RAM contention. Plan 93.)
            constexpr int kPoolCap  = 24;   // CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT
            constexpr int kReserve  = 4;    // keep slots free for CLI/control replies
            const int chunks = (int)((len + chunk - 1) / chunk);
            if (ble_->txPending() + chunks > kPoolCap - kReserve) return true;  // no room → skip
        }
        // A single BLE notification carries only (ATT_MTU − 3) bytes, so split the frame
        // across notifications; the host's PLP FrameParser reassembles the byte stream.
        for (size_t off = 0; off < len; off += chunk) {
            const size_t n = (len - off < chunk) ? (len - off) : chunk;
            if (!ble_->notify(plp_ble::CHAR_TX, data + off, n)) return false;
        }
        return true;
    }
    void onRecv(RecvFn fn, void* user) override { recv_ = fn; user_ = user; }
    bool isConnected() const override { return ble_ && ble_->isConnected(); }
    // BLE 4.2+ ATT MTU is negotiated; 180 stays safely under the common 185-byte
    // payload after the 3-byte ATT header on a 23..247 MTU link.
    size_t mtu() const override { return 180; }

private:
    static void writeThunk(void* user, const char* uuid, const uint8_t* d, size_t n) {
        auto* self = static_cast<BleLinkTransport*>(user);
        // Only the RX characteristic carries inbound PLP; ignore other writes.
        if (std::strcmp(uuid, plp_ble::CHAR_RX) != 0) return;
        if (self->recv_) self->recv_(self->user_, d, n);
    }

    IBleAdapter* ble_  = nullptr;
    RecvFn       recv_ = nullptr;
    void*        user_ = nullptr;
};

} // namespace nema
