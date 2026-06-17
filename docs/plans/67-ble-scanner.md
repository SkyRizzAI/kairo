# 67 — BLE Scanner App

> BLE central mode: scan perangkat BLE sekitar, tampilkan MAC/name/RSSI,
> koneksi ke peripheral, GATT service/characteristic discovery.

- Status: 🔴 Not started
- Depends on: 34 (Bluetooth Core BLE), 28 (SkyRizz E32 — ESP32-S3 BLE),
  42 (Capability Model)
- Blocks: —

---

## 1. Goals

1. BLE scanning (active, durasi configurable)
2. Tampilkan daftar perangkat: MAC, name, RSSI (dBm), service UUIDs
3. Live update — hasil scan muncul real-time (setiap advertisement diterima)
4. Tap perangkat → connect + GATT service discovery
5. Browse characteristics (UUID, properties: read/write/notify)
6. Capability gate: `bt.ble.central` di manifest `needs[]`

## 2. Arsitektur

### HAL Extension — IBleAdapter

`IBleAdapter` dari Plan 34 sudah punya primitif peripheral role. Tambah central:

```cpp
// hal/ble_adapter.h (extend)
struct BleScanResult {
    std::string mac;
    std::string name;
    int8_t rssi;
    std::vector<std::string> serviceUuids;
    bool connectable;
};

class IBleAdapter {
public:
    // ... existing (peripheral) ...
    
    // Central mode (NEW)
    using ScanCallback = std::function<void(const BleScanResult&)>;
    virtual void startScan(uint32_t durationMs, ScanCallback onResult) = 0;
    virtual void stopScan() = 0;
    virtual bool isScanning() = 0;
    
    virtual void connect(const std::string& mac) = 0;
    virtual void disconnect(const std::string& mac) = 0;
    
    struct GattService {
        std::string uuid;
        std::vector<GattCharacteristic> characteristics;
    };
    struct GattCharacteristic {
        std::string uuid;
        uint8_t properties; // read/write/notify/indicate
        std::vector<uint8_t> value;
    };
    
    using ServicesDiscoveredCallback = std::function<void(const std::string& mac, const std::vector<GattService>&)>;
    virtual void discoverServices(const std::string& mac, ServicesDiscoveredCallback onDone) = 0;
};
```

### NimBLE Implementation (ESP32)

NimBLE sudah punya `ble_gap_disc()`:

```cpp
// platforms/esp32/src/esp32_ble_adapter.cpp
void Esp32BleAdapter::startScan(uint32_t durationMs, ScanCallback onResult) {
    ble_gap_disc_params_t params = {
        .itvl = 0, .window = 0, // default
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WA,
        .limited = 0,
        .passive = 0, // active scan (dapat nama device)
        .filter_duplicates = 1
    };
    scanCallback_ = std::move(onResult);
    ble_gap_disc(ownAddrType_, durationMs, &params, onDiscEvent, this);
}
```

### BleScannerApp

```
┌──────────────────────────────┐
│  BLE Scanner          [Scan] │
│                               │
│  Scanning... 3s               │
│                               │
│  ▸ Device A        -45 dBm   │
│    AA:BB:CC:DD:EE:FF          │
│    0x1800, 0x180A             │
│                               │
│    Device B        -67 dBm   │
│    "My Speaker"               │
│    AA:11:22:33:44:55          │
│    0x1800, 0x180A, 0x1101     │
│                               │
│  [Select] Connect  [Back]    │
└──────────────────────────────┘
```

- Auto-scan 5 detik saat app dibuka
- Tekan Select untuk rescan manual
- List diurutkan RSSI (terkuat di atas)
- Tap item → connect → discover services → tampilkan characteristic tree

### Characteristic Browser

```
┌──────────────────────────────┐
│  Device A > Services          │
│                               │
│  ▸ Generic Access (0x1800)   │
│    ├─ Device Name (READ)     │
│    └─ Appearance (READ)      │
│                               │
│  ▸ Battery Service (0x180F)  │
│    └─ Battery Level (R,N)   │
│        Value: 85%            │
│                               │
│  [Back]  [Read]  [Notify]    │
└──────────────────────────────┘
```

- Tree view: expand service → lihat characteristics
- Read: kirim GATT read request, tampilkan nilai (hex + ASCII interpretasi)
- Notify toggle: subscribe/unsubscribe characteristic notification

### JS API

```js
import { ble } from "nema";

// Scan
const devices = await ble.scan(5000);
// [{ mac: "AA:BB:CC:DD:EE:FF", name: "Device A", rssi: -45, ... }]

// Connect + list services
const services = await ble.connect("AA:BB:CC:DD:EE:FF");
// [{ uuid: "1800", characteristics: [...] }]

// Read characteristic
const value = await ble.read("AA:BB:CC:DD:EE:FF", "1800", "2A00");
// Uint8Array

// Notify
ble.onNotify("AA:BB:CC:DD:EE:FF", "180F", "2A19", (value) => {
    console.log("Battery:", value[0], "%");
});
```

### Capability Gate

App butuh `"bt.ble.central"` di manifest `needs[]` untuk akses:
- `ble.scan()`
- `ble.connect()`
- `ble.read()`
- `ble.onNotify()`

BLE peripheral (Plan 34) sudah pakai cap `"bt.ble"` — central butuh cap
terpisah karena lebih powerful (bisa scan semua device sekitar).

## 3. Implementasi

### Fase 1 — HAL + NimBLE scan (1.5 hari)

1. Extend `IBleAdapter` dengan central methods
2. Implementasi `startScan/stopScan` di `Esp32BleAdapter` (NimBLE `ble_gap_disc`)
3. `BleScanResult` struct + callback
4. Verifikasi: scan mendeteksi device BLE sekitar (mouse, keyboard, phone)

### Fase 2 — Connect + GATT (1.5 hari)

1. `connect(mac)` → NimBLE `ble_gap_connect`
2. `discoverServices(mac)` → NimBLE GATT discovery
3. Read/write/notify per characteristic

### Fase 3 — BleScannerApp UI (1 hari)

1. App screen: scan results list + RSSI bars
2. Auto-scan 5s on open, manual rescan button
3. Tap → connect → characteristic browser
4. Read/notify interaction

### Fase 4 — JS API (0.5 hari)

1. `nema.ble` IDL definition (extend existing BLE IDL jika ada)
2. Codegen bridge
3. Capability gate `bt.ble.central`

## 4. Files

| File | Perubahan |
|------|-----------|
| `firmware/core/include/nema/hal/ble_adapter.h` | Extend `IBleAdapter` central |
| `firmware/platforms/esp32/src/esp32_ble_adapter.cpp` | Implementasi scan/connect |
| `firmware/core/src/apps/ble_scanner_app.cpp` | **Baru** — BleScannerApp |
| `firmware/core/include/nema/apps/ble_scanner_app.h` | **Baru** — header |
| `firmware/core/src/apps/ble_characteristic_browser.cpp` | **Baru** — GATT browser |
| `packages/nema-sdk/src/idl/ble.idl` | Extend JS API central |

## 5. Acceptance Criteria

- [ ] Scan 5 detik → daftar device BLE muncul (RSSI + name + MAC)
- [ ] List terurut RSSI terkuat di atas
- [ ] Connect ke peripheral → discover service + characteristic
- [ ] Read characteristic tampilkan nilai (hex + ASCII)
- [ ] Notify characteristic terima update real-time
- [ ] App tanpa `bt.ble.central` cap tidak bisa scan/connect
- [ ] `nema.ble.scan(5000)` berfungsi dari JS app (simulator + hardware)
- [ ] Build hijau: ESP32 (NimBLE central + GATT)
