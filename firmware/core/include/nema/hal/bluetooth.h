#pragma once
#include "nema/hal/driver.h"
#include <cstdint>
#include <cstddef>

// Connectivity foundation — Bluetooth/BLE (Plan 34, Layer 1).
//
// One unified controller hosts two sub-adapters (BLE + Classic), selected by
// CAPABILITY, never by board type. ESP32-S3 is BLE-only, so IClassicAdapter is
// declared-but-unimplemented there. The BLE adapter is a general GATT peripheral:
// the remote layer (Plan 35) is ONE consumer (its PLP service); apps may register
// their own services for data exchange.
namespace nema {

enum class BtMode : uint8_t { Off, Ble, Classic, Dual };

// Peer identity. POD — safe to copy across threads.
struct BtPeer {
    char    name[32] = {};
    uint8_t addr[6]  = {};
    bool    bonded   = false;
};

// Pairing confirmation request (numeric comparison / passkey display).
struct BlePairRequest {
    uint32_t passkey = 0;     // 6-digit code to show on screen
    uint8_t  addr[6] = {};
};

// GATT server definition (peripheral role). Property bitmask.
namespace BleProp { enum : uint8_t { Read = 1, Write = 2, Notify = 4 }; }
struct BleCharacteristic { const char* uuid; uint8_t props; };
struct BleService        { const char* uuid; const BleCharacteristic* chars; uint8_t charCount; };

// ── Radio controller (shared by BLE + Classic) ──
struct IBluetoothController : IDriver {
    DriverKind kind() const override { return DriverKind::Bluetooth; }
    // enable() may init a heavy stack — call it from a TaskRunner worker.
    virtual bool        enable(BtMode mode) = 0;
    virtual void        disable() = 0;
    virtual bool        isEnabled() const = 0;
    virtual BtMode      mode() const = 0;
    virtual const char* address() const = 0;          // local MAC "AA:BB:..."
    virtual void        setDeviceName(const char* name) = 0;
    virtual const char* deviceName() const = 0;
};

// ── BLE peripheral adapter (advertise + GATT server + pairing + bonding) ──
struct IBleAdapter : IDriver {
    DriverKind kind() const override { return DriverKind::Bluetooth; }

    // GATT server definition. Called once per service before advertising; the
    // remote layer registers its PLP service, apps may register their own.
    virtual void registerService(const BleService& svc) = 0;

    // Advertising.
    virtual bool startAdvertising() = 0;
    virtual void stopAdvertising()  = 0;
    virtual bool isAdvertising() const = 0;

    // Connection state.
    virtual bool isConnected() const = 0;
    virtual bool peer(BtPeer& out) const = 0;
    virtual void disconnect() = 0;

    // I/O primitives (server side) — used by the remote layer's BleLinkTransport.
    virtual bool notify(const char* charUuid, const uint8_t* data, size_t len) = 0;
    // Outstanding (submitted-but-not-yet-transmitted) TX notifications. Lets the transport
    // pace the heavy screen mirror at frame granularity. Default 0 = no pacing (Plan 93).
    virtual int  txPending() const { return 0; }
    using WriteFn = void (*)(void* user, const char* charUuid, const uint8_t* data, size_t len);
    virtual void onWrite(WriteFn fn, void* user) = 0;

    // Pairing: stack raises a request → app shows passkey → confirmPairing().
    using PairFn = void (*)(void* user, const BlePairRequest& req);
    virtual void onPairRequest(PairFn fn, void* user) = 0;
    virtual void confirmPairing(bool accept) = 0;

    // Bonded peers (persisted in NVS by the stack).
    virtual size_t bondedCount() const = 0;
    virtual bool   bondedAt(size_t i, BtPeer& out) const = 0;
    virtual void   forget(const uint8_t addr[6]) = 0;
    virtual void   forgetAll() = 0;

    // ── Central role (Plan 67) — optional, default no-ops ──

    // Scan result delivered per advertisement.
    struct ScanResult {
        char    mac[18] = {};       // "AA:BB:CC:DD:EE:FF"
        char    name[32] = {};
        int8_t  rssi = 0;
        bool    connectable = false;
    };
    using ScanCallback = void (*)(void* user, const ScanResult& r);

    virtual bool startScan(uint32_t durationMs, ScanCallback cb, void* user) { (void)durationMs; (void)cb; (void)user; return false; }
    virtual void stopScan()  {}
    virtual bool isScanning() const { return false; }

    // Connect to a scanned peripheral.
    virtual bool connectTo(const char* mac) { (void)mac; return false; }
    // Disconnect a central connection.
    virtual void disconnectFrom(const char* mac) { (void)mac; }
};

// ── Classic adapter — interface only (future; no impl on ESP32-S3) ──
struct IClassicAdapter : IDriver {
    DriverKind kind() const override { return DriverKind::Bluetooth; }
    virtual bool startDiscoverable() = 0;   // SPP/A2DP — future
    virtual void stopDiscoverable()  = 0;
};

} // namespace nema
