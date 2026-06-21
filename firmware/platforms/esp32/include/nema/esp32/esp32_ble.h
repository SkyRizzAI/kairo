#pragma once
#include "nema/hal/bluetooth.h"
#include "nema/service.h"
#include <string>
#include <vector>
#include <cstdint>

namespace nema {

class Logger;
class EventBus;
class AsyncEventPoster;
class CapabilityRegistry;
class Runtime;

// Esp32Ble — NimBLE-backed BLE peripheral for ESP32-S3 (Plan 34). Implements both
// the radio controller and the GATT-server adapter (NimBLE is a single stack).
// LE Secure Connections + numeric comparison; bonds persist in NVS. Host-stack
// callbacks are marshalled to the main task via AsyncEventPoster.
//
// NOTE: code path is guarded by CONFIG_BT_NIMBLE_ENABLED; when BT is disabled in
// sdkconfig the methods degrade to no-ops so non-BT boards still link.
class Esp32Ble : public IBluetoothController, public IBleAdapter, public IService {
public:
    void onRegister(Runtime& rt) override;

    // IDriver / IService
    const char* name() const override { return "Esp32Ble"; }
    void start() override {}
    void stop()  override {}

    // ── IBluetoothController ──
    bool        enable(BtMode mode) override;
    void        disable() override;
    bool        isEnabled() const override { return enabled_; }
    BtMode      mode() const override { return mode_; }
    const char* address() const override { return addr_.c_str(); }
    void        setDeviceName(const char* n) override { devName_ = n ? n : "Palanu"; }
    const char* deviceName() const override { return devName_.c_str(); }

    // ── IBleAdapter ──
    void registerService(const BleService& svc) override;
    bool startAdvertising() override;
    void stopAdvertising()  override;
    bool isAdvertising() const override { return advertising_; }
    bool isConnected() const override { return connHandle_ != 0xFFFF; }
    bool peer(BtPeer& out) const override;
    void disconnect() override;
    bool notify(const char* charUuid, const uint8_t* data, size_t len) override;
    void onWrite(WriteFn fn, void* user) override { writeFn_ = fn; writeUser_ = user; }
    void onPairRequest(PairFn fn, void* user) override { pairFn_ = fn; pairUser_ = user; }
    void confirmPairing(bool accept) override;
    size_t bondedCount() const override;
    bool   bondedAt(size_t i, BtPeer& out) const override;
    void   forget(const uint8_t addr[6]) override;
    void   forgetAll() override;

    // ── IBleAdapter central role (Plan 67) ──
    bool startScan(uint32_t durationMs, ScanCallback cb, void* user) override;
    void stopScan() override;
    bool isScanning() const override { return scanning_; }
    bool connectTo(const char* mac) override;
    void disconnectFrom(const char* mac) override;

    // Called from the NimBLE host task (internal).
    int  onGapEvent(void* event);
    void onSync();
    void onRxWrite(const uint8_t* data, size_t len);   // GATT RX write → writeFn_

private:
    void startAdvertisingInternal();

    Logger*             log_    = nullptr;
    EventBus*           events_ = nullptr;
    AsyncEventPoster*   poster_ = nullptr;
    CapabilityRegistry* caps_   = nullptr;

    bool        enabled_     = false;
    bool        advertising_ = false;
    BtMode      mode_        = BtMode::Off;
    std::string addr_        = "00:00:00:00:00:00";
    std::string devName_     = "Palanu";

    uint16_t connHandle_ = 0xFFFF;     // 0xFFFF = not connected
    BtPeer   peer_{};

    // pending numeric-comparison passkey
    bool     pairing_ = false;
    uint32_t pendingPasskey_ = 0;

    WriteFn writeFn_ = nullptr; void* writeUser_ = nullptr;
    PairFn  pairFn_  = nullptr; void* pairUser_  = nullptr;

    // Central role (Plan 67)
    bool         scanning_ = false;
    bool         connecting_ = false;
    ScanCallback scanCb_ = nullptr;
    void*        scanCbUser_ = nullptr;
    uint16_t     centConnHandle_ = 0xFFFF;
};

} // namespace nema
