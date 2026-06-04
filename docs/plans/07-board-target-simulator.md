# 07 — Simulator Board, Target & Sample Services

> Merakit semuanya: **Simulator Board** (deklarasi hardware virtual), **Simulator Target** (entry point `main()` final untuk MVP), plus contoh **background service** (Clock) — sehingga runtime benar-benar **boot penuh & berjalan** di host.

- Status: ☐ Not started
- Milestone: M3 (Simulator)
- Depends on: 05, 06
- Blocks: 08, 09

---

## Goal

- `SimulatorBoard` yang mendeklarasikan hardware virtual yang "terpasang" (battery, wifi) — input untuk Hardware/Capability Registry (stage 08).
- `targets/simulator/main.cpp` final: rakit `SimulatorPlatform` + `SimulatorBoard` + `Runtime`, jalankan boot flow penuh & `run()` loop.
- Contoh background service `ClockService` (§17 master plan) yang tick tiap detik & publish event — bukti "banyak service jalan bersamaan".

## Scope

### In scope

- `SimulatorBoard : IBoard` di `firmware/boards/simulator/`.
- `ClockService : IService` (di core `services/` atau di target sebagai contoh) — tick 1 Hz, publish event/log waktu.
- Final wiring `main.cpp`: konstruksi objek (ownership di main), jalankan boot flow, tangani shutdown (mis. setelah N detik atau sinyal — stdin command nanti stage 09).
- CMake: target link core + platform sim + board sim.

### Out of scope

- Kairo Dev Board (ESP32-S3) / Kairo Board V1 (non-MVP).
- Home screen / UI (non-MVP).
- Driver baru (sudah di stage 06).

---

## Design

### File

```text
firmware/boards/simulator/
├─ include/kairo/sim/simulator_board.h
├─ src/simulator_board.cpp
└─ CMakeLists.txt
firmware/core/include/kairo/services/clock_service.h   # contoh background service
firmware/core/src/services/clock_service.cpp
firmware/targets/simulator/main.cpp                     # final wiring
```

### SimulatorBoard

```cpp
class SimulatorBoard : public IBoard {
public:
  const char* name() const override { return "simulator"; }
  void describeHardware(Runtime& rt) override {
    // daftarkan ke Hardware Registry (stage 08): Battery, WiFi
    // daftarkan ke Capability Registry: "battery", "wifi"
    // (stage 08 menyediakan API registry; di sini pemanggilnya)
  }
};
```

### ClockService (contoh background service)

```cpp
class ClockService : public IService {
public:
  ClockService(Logger& log, EventBus& bus) : log_(log), bus_(bus) {}
  const char* name() const override { return "ClockService"; }
  void start() override { log_.info("ClockService", "started"); }
  void stop()  override { log_.info("ClockService", "stopped"); }
  void tick(uint64_t nowMs) override {
    if (nowMs - last_ >= 1000) { last_ = nowMs;
      bus_.publish({ "ClockTick", { {"uptimeMs", std::to_string(nowMs)} } });
    }
  }
private:
  Logger& log_; EventBus& bus_; uint64_t last_ = 0;
};
```

> `ClockTick` adalah event lokal contoh (boleh ditambah ke daftar konstanta, atau dibiarkan ad-hoc). Tujuannya membuktikan event stream "hidup" di panel Events nanti.

### `main.cpp` final (sketsa)

```cpp
int main() {
  SimulatorPlatform platform;
  SimulatorBoard    board;

  Runtime rt = Runtime::create();
  rt.loadPlatform(platform);          // BootPhase::PlatformLoaded
  rt.loadBoard(board);                // BootPhase::BoardLoaded
  rt.initCore();                      // Logger, EventBus, Container, Manager, (Registry stage 08)
  rt.registerServices();             // platform.registerDrivers + board.describeHardware
                                      // + daftarkan ClockService
  rt.start();                         // ServiceManager.startAll → SystemReady
  rt.run();                           // loop tick sampai requestShutdown (stage 09: dari stdin)
  return 0;
}
```

> Catatan urutan: `initCore` sebelum `registerServices` agar Logger/EventBus/Registry sudah siap saat driver mendaftar. (Sesuaikan dengan boot flow stage 02; konsistenkan.)

### Penghentian (sementara, sebelum stage 09)

Karena command stdin baru ada di stage 09, untuk verifikasi stage ini boleh:
- jalankan loop dengan batas waktu/iterasi (mis. `requestShutdown()` setelah ~5 dtk) **untuk uji lokal saja**, atau
- biarkan jalan sampai Ctrl-C. Hapus batasan sementara saat stage 09 menyambungkan shutdown via command.

---

## Tasks

- [ ] `SimulatorBoard` + `describeHardware()` (pemanggil registry; isi API-nya nyusul stage 08 — boleh stub log dulu).
- [ ] `ClockService` (tick 1 Hz, publish event).
- [ ] Daftarkan `ClockService` di `registerServices`.
- [ ] `main.cpp` final wiring (platform+board+runtime).
- [ ] CMake board sim + link di target.
- [ ] Verifikasi boot penuh & service jalan bersamaan.

## Acceptance criteria

- Boot mencapai `SystemReady`; log menampilkan urutan boot flow lengkap.
- Minimal 3 service `Running` bersamaan: `SimBatteryDriver`, `SimWifiDriver`, `ClockService`.
- `ClockTick` muncul ~1×/detik; `BatteryChanged` muncul periodik — keduanya via Event Bus.
- Shutdown bersih → semua service `Stopped` (reverse order).

## How to verify

```bash
bun run build:firmware && bun run run:firmware
# amati boot flow penuh, 3+ service Running, ClockTick tiap detik
```

## Risks / notes

- Ownership semua objek ada di `main()` (stack). Pastikan lifetime objek > runtime (mereka hidup sepanjang `main`). Tidak ada `new`/heap untuk top-level objek = aman & sederhana.
- Jangan biarkan batas-waktu shutdown sementara nyangkut ke MVP final — hapus saat stage 09.
