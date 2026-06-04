# 06 — HAL & Simulator Platform (Dummy Drivers)

> Hardware Abstraction Layer: interface driver yang **core kenal**, lalu **Simulator Platform** yang menyediakan **dummy driver** sehingga runtime bisa jalan tanpa hardware (§9, §3 "Hardware Agnostic / Capability Driven").

- Status: ☐ Not started
- Milestone: M3 (Simulator) + fondasi HAL
- Depends on: 03, 04, 05
- Blocks: 07, 08

---

## Goal

- Interface driver abstrak di `core/` (core tak tahu implementasi konkret).
- `SimulatorPlatform` (di `firmware/platforms/simulator/`) yang membuat **dummy driver** dan mendaftarkannya ke `Runtime` saat `registerDrivers()`.
- Dummy driver yang "hidup": memancarkan event & log (mensimulasikan perilaku hardware) agar simulator terasa nyata.

## Scope

### In scope

- HAL interface (di core): `IBatteryDriver`, `IWifiDriver` (minimal, capability-driven). Plus base `IDriver` (name + kind).
- `IClock` implementasi nyata untuk host: `HostClock` (pakai `std::chrono`, **di platform**, bukan core).
- `SimulatorPlatform`:
  - `HostClock`
  - `SimBatteryDriver` — level turun perlahan, publish `BatteryChanged`.
  - `SimWifiDriver` — bisa connect/disconnect, publish `NetworkConnected/Disconnected`.
- Driver dibungkus service (atau ber-`tick`) sehingga ikut lifecycle Service Manager.

### Out of scope

- Display/NFC/RFID/IR/SubGHz/Audio driver (non-MVP — cukup deklarasi interface kalau perlu, implementasi nanti).
- Driver ESP32 nyata (platform esp32 ditunda total).
- Network/Display/Storage **Manager** lengkap (§7) — MVP cukup driver + registry (stage 08); manager pass-through ditunda.

---

## Design

### File

```text
firmware/core/include/kairo/hal/
├─ driver.h           # IDriver (base): name(), kind()
├─ battery.h          # IBatteryDriver: level(), isCharging()
└─ wifi.h             # IWifiDriver: connect(ssid), disconnect(), isConnected()
firmware/platforms/simulator/
├─ include/kairo/sim/simulator_platform.h
├─ src/simulator_platform.cpp
├─ src/host_clock.cpp           # IClock via std::chrono
├─ src/sim_battery_driver.cpp
├─ src/sim_wifi_driver.cpp
└─ CMakeLists.txt               # add_library(kairo_platform_sim) link kairo_core, kairo_vendor
```

### HAL interface (core, agnostic)

```cpp
// driver.h
namespace kairo {
enum class DriverKind { Battery, Wifi, Bluetooth, Display, Storage, /* ... */ Other };
struct IDriver {
  virtual ~IDriver() = default;
  virtual const char* name() const = 0;
  virtual DriverKind kind() const = 0;
};

// battery.h
struct IBatteryDriver : IDriver {
  virtual int  level() const = 0;      // 0..100
  virtual bool isCharging() const = 0;
};

// wifi.h
struct IWifiDriver : IDriver {
  virtual bool connect(const char* ssid) = 0;
  virtual void disconnect() = 0;
  virtual bool isConnected() const = 0;
};
}
```

### SimulatorPlatform

```cpp
// implements IPlatform (stage 02)
class SimulatorPlatform : public IPlatform {
public:
  const char* name() const override { return "simulator"; }
  IClock& clock() override { return clock_; }
  void registerDrivers(Runtime& rt) override {
    // buat dummy drivers, register ke container, daftarkan capability+hardware (stage 08)
    rt.container().registerService(&battery_);  // jika driver juga IService
    rt.container().registerService(&wifi_);
    // capabilities.add("wifi"); hardwareRegistry.add(...) → stage 08
  }
private:
  HostClock clock_;
  SimBatteryDriver battery_;
  SimWifiDriver    wifi_;
};
```

### Dummy driver behavior

- `SimBatteryDriver` : implement `IBatteryDriver` **dan** `IService` → di `tick(now)`, tiap ~5 dtk turunkan level 1%, publish `BatteryChanged{level}` + log debug. Mulai 100%, charging=false.
- `SimWifiDriver` : implement `IWifiDriver` **dan** `IService`. Default disconnected. `connect()` set connected + publish `NetworkConnected{ssid}`. `disconnect()` → `NetworkDisconnected`. (Dipicu dari Controls panel via command di stage 09/10, atau auto-connect saat start untuk demo.)

> Driver butuh akses Logger & EventBus → injeksikan saat konstruksi dari platform (platform pegang ref-nya dari `Runtime`, atau diberikan saat `registerDrivers`). Pilih satu pola & konsisten (rekomendasi: simpan `Runtime*`/`Logger&`/`EventBus&` di driver via setter saat register).

### Update target & CMake

- `targets/simulator/main.cpp`: ganti stub platform jadi `SimulatorPlatform` nyata.
- `targets/simulator/CMakeLists.txt`: link `kairo_platform_sim`.

---

## Tasks

- [ ] HAL interface di core: `driver.h`, `battery.h`, `wifi.h`.
- [ ] `HostClock` (IClock via `std::chrono`) di platform sim.
- [ ] `SimulatorPlatform` + `registerDrivers()`.
- [ ] `SimBatteryDriver` & `SimWifiDriver` (IDriver+IService, emit event+log).
- [ ] `CMakeLists.txt` platform sim; link dari target; pakai platform nyata di `main.cpp`.
- [ ] Verifikasi event hardware mengalir saat boot/tick.

## Acceptance criteria

- Boot: kedua driver terdaftar & masuk `Running`.
- Selang beberapa detik, `BatteryChanged` ter-publish (level turun) → terlihat di log.
- Memanggil `wifi.connect("Office")` (sementara bisa di-hardcode di demo) → `NetworkConnected{ssid=Office}`.
- `firmware/core/**` tetap tidak tahu kelas `Sim*` apa pun (hanya interface). `std::chrono` hanya di platform.

## How to verify

```bash
bun run build:firmware && bun run run:firmware
# log memperlihatkan driver start + BatteryChanged periodik + (demo) NetworkConnected
```

## Risks / notes

- Pola injeksi Logger/EventBus ke driver: hindari global singleton. Lebih bersih lewat konstruktor/`init(Runtime&)`. Tetapkan satu pola di awal.
- Menggabung "driver" dan "service" pada satu kelas itu pragmatis untuk MVP. Kalau mau pisah (driver murni + service pembungkus), boleh — tapi tambah boilerplate. Untuk MVP, gabung diperbolehkan & didokumentasikan.
