# 08 — System Introspection

> SystemInfo + Hardware Registry + Capability Registry (§8 master plan). Membuat runtime "mudah diinspeksi" (Observability First) dan menegakkan pola **capability-driven**.

- Status: ☐ Not started
- Milestone: M2 (Observability)
- Depends on: 06, 07
- Blocks: 09 (snapshot dikirim ke web), 10 (data dipakai UI)

---

## Goal

- `SystemInfo`: info runtime (build/firmware version, platform, board, dan—untuk host—angka memori sederhana/placeholder).
- `HardwareRegistry`: daftar hardware yang terpasang (Display, WiFi, Battery, dst — untuk MVP: yang dummy ada).
- `CapabilityRegistry`: daftar capability (`wifi`, `battery`, `networking`, ...). App/plugin cek capability, **bukan** tipe board.
- Registry diisi oleh Platform (`registerDrivers`) & Board (`describeHardware`).

## Scope

### In scope

- `SystemInfo` struct + sumber datanya (versi dari macro build, platform/board name dari objek aktif).
- `HardwareRegistry` & `CapabilityRegistry` (add/has/list).
- Integrasi: Board `describeHardware` mendaftar hardware; Platform/driver mendaftar capability.
- API `capabilities.has("wifi")` yang rapi (sesuai contoh §3).
- Snapshot serializable (untuk stage 09 dikirim sekali saat boot).

### Out of scope

- Panel Hardware/Capability di web (tidak termasuk 4 panel MVP). Data disiapkan; panel-nya nanti.
- Angka CPU/RAM/PSRAM/Flash akurat dari hardware (host pakai placeholder/`0`/string "host"). Akurasi ESP32 nanti.

---

## Design

### File

```text
firmware/core/include/kairo/system/
├─ system_info.h
├─ hardware_registry.h
└─ capability_registry.h
firmware/core/src/system/
├─ hardware_registry.cpp
└─ capability_registry.cpp
```

### Sketsa

```cpp
// system_info.h
namespace kairo {
struct SystemInfo {
  std::string buildVersion    = KAIRO_BUILD_VERSION;   // -D dari CMake
  std::string firmwareVersion = KAIRO_FW_VERSION;
  std::string platformName;    // "simulator"
  std::string boardName;       // "simulator"
  // host: nilai memori placeholder; ESP32 isi nyata nanti
  uint32_t cpuMhz = 0;
  uint32_t ramKb  = 0;
  uint32_t psramKb = 0;
  uint32_t flashKb = 0;
};

// hardware_registry.h
struct HardwareEntry { std::string id; DriverKind kind; std::string detail; };
class HardwareRegistry {
public:
  void add(HardwareEntry e);
  bool has(DriverKind kind) const;
  const std::vector<HardwareEntry>& list() const;
};

// capability_registry.h
class CapabilityRegistry {
public:
  void add(std::string capability);          // "wifi", "battery", "networking"
  bool has(const std::string& cap) const;    // capabilities.has("wifi")
  const std::vector<std::string>& list() const;
};
}
```

### Integrasi

- `Runtime` memegang `SystemInfo`, `HardwareRegistry&`, `CapabilityRegistry&` (akses via getter).
- `initCore()`: isi `SystemInfo` (platform/board name dari objek aktif; versi dari macro).
- `SimulatorBoard::describeHardware(rt)`:
  - `rt.hardware().add({"battery", DriverKind::Battery, "virtual 100%"})`
  - `rt.hardware().add({"wifi", DriverKind::Wifi, "virtual"})`
  - `rt.capabilities().add("battery"); rt.capabilities().add("wifi"); rt.capabilities().add("networking");`
- Versi via CMake: `target_compile_definitions(... KAIRO_BUILD_VERSION="0.1.0-mvp" KAIRO_FW_VERSION="0.1.0")`.

### Demonstrasi capability-driven

Di boot (atau di sebuah service contoh), tunjukkan pola benar:

```cpp
if (rt.capabilities().has("wifi")) {
  // aktifkan fitur wifi-dependent
}
```

dan log daftar capability saat boot untuk observability.

---

## Tasks

- [ ] `SystemInfo` + macro versi via CMake.
- [ ] `HardwareRegistry` (add/has/list) + `CapabilityRegistry` (add/has/list).
- [ ] Akses dari `Runtime`; isi di `initCore`/`describeHardware`/`registerDrivers`.
- [ ] Log snapshot saat boot (hardware list + capability list).
- [ ] Sediakan method snapshot→struct ringkas (dipakai stage 09 untuk dikirim ke web).
- [ ] Contoh penggunaan `capabilities.has("wifi")` di kode boot/service.

## Acceptance criteria

- Saat boot, log menampilkan SystemInfo (platform=simulator, board=simulator, versi terisi) + daftar hardware (battery, wifi) + daftar capability.
- `capabilities.has("wifi")` → true; `has("nfc")` → false (NFC tak terpasang di sim MVP).
- Tidak ada cek tipe board/hardware bergaya `isEsp32()` di mana pun (grep bersih) — semua via capability/registry.

## How to verify

```bash
bun run build:firmware && bun run run:firmware
# log menampilkan SystemInfo + Hardware Registry + Capability Registry saat boot
```

## Risks / notes

- Untuk host, angka memori placeholder — beri komentar jelas agar tak disangka akurat. Ketika platform ESP32 dibuat, isi dari API SDK.
- `DriverKind` dipakai bersama HAL (stage 06) → jaga satu sumber enum (di `hal/driver.h`).
