# 02 — Core Runtime & Boot Flow

> Skeleton Core Runtime: interface dasar yang dipakai seluruh sistem, objek `Runtime`, dan urutan boot (§12 master plan). Belum ada service konkret — itu stage 03+.

- Status: ☐ Not started
- Milestone: M1 (Core Foundation)
- Depends on: 01
- Blocks: 03, 04, 05, 06, 07, 08, 09

---

## Goal

Menyediakan "tulang punggung" runtime yang hardware-agnostic:

- Interface inti (`IService`, `IPlatform`, `IBoard`, `IClock`) di `firmware/core/include/kairo/`.
- Objek `Runtime` yang menyimpan referensi ke Platform, Board, dan registry service.
- Implementasi **Boot Flow** sesuai master plan §12 (sampai "Start Runtime"; "Launch Home Screen" di-skip karena UI Runtime di luar MVP).
- `targets/simulator/main.cpp` dirombak memanggil boot flow (masih pakai platform/board stub minimal sampai stage 06/07 mengisinya).

## Scope

### In scope

- Tipe dasar & enum: `LogLevel` (dipakai stage 03), `ServiceState`, `BootPhase`.
- Interface: `IPlatform`, `IBoard`, `IService`, `IClock` (abstraksi waktu agar core tak pakai `std::chrono` langsung di logika — opsional tapi rapi).
- `Runtime` class: lifecycle `create → loadPlatform → loadBoard → registerServices → initCore → start`.
- Stub `IPlatform`/`IBoard` minimal di core untuk testing (real impl di stage 06/07).

### Out of scope

- Logger/EventBus/Container konkret (stage 03/04/05) — di sini hanya forward-declare / interface yang dibutuhkan.
- Launch Home Screen (UI Runtime — non-MVP). Boot flow berhenti di "Start Runtime".

---

## Design

### Boot Flow (master plan §12, versi MVP)

```text
main()
  → Runtime::create()
  → Runtime::loadPlatform(platform)
  → Runtime::loadBoard(board)
  → Runtime::registerServices()      // platform & board mendaftarkan service/driver
  → Runtime::initCore()              // resolve & init core services berurutan
  → Runtime::start()                 // start semua service (Service Manager)
  → [Launch Home Screen]             // ⛔ di-skip di MVP (no UI runtime)
  → Runtime::run()                   // loop utama: tick services sampai shutdown
```

### File baru

```text
firmware/core/include/kairo/
├─ types.h            # LogLevel, ServiceState, BootPhase, dsb
├─ clock.h            # IClock (now(), monotonic millis)
├─ service.h          # IService
├─ platform.h         # IPlatform
├─ board.h            # IBoard
└─ runtime.h          # Runtime
firmware/core/src/
└─ runtime.cpp
```

### Interface sketsa

```cpp
// types.h
namespace kairo {
enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };
enum class ServiceState { Created, Starting, Running, Stopping, Stopped, Failed };
enum class BootPhase { None, PlatformLoaded, BoardLoaded, ServicesRegistered, CoreReady, Running };
}

// clock.h — core tak pakai std::chrono langsung; platform yang sediakan
namespace kairo {
struct IClock {
  virtual ~IClock() = default;
  virtual uint64_t millis() = 0;          // monotonic ms sejak boot
  virtual uint64_t epochMs() = 0;         // wall-clock (untuk timestamp log)
};
}

// service.h
namespace kairo {
struct IService {
  virtual ~IService() = default;
  virtual const char* name() const = 0;
  virtual void start() = 0;               // dipanggil Service Manager
  virtual void stop()  = 0;
  virtual void tick(uint64_t nowMs) {}    // optional, untuk background service
};
}

// platform.h — abstraksi environment (simulator/esp32)
namespace kairo {
class Runtime;
struct IPlatform {
  virtual ~IPlatform() = default;
  virtual const char* name() const = 0;   // "simulator"
  virtual IClock& clock() = 0;
  virtual void registerDrivers(Runtime& rt) = 0;  // diisi stage 06
};
}

// board.h
namespace kairo {
struct IBoard {
  virtual ~IBoard() = default;
  virtual const char* name() const = 0;   // "simulator"
  virtual void describeHardware(Runtime& rt) = 0;  // diisi stage 08
};
}

// runtime.h
namespace kairo {
class Runtime {
public:
  static Runtime create();
  void loadPlatform(IPlatform& p);
  void loadBoard(IBoard& b);
  void registerServices();
  void initCore();
  void start();
  void run();                  // loop tick sampai requestShutdown()
  void requestShutdown();

  IPlatform& platform();
  IClock& clock();
  BootPhase phase() const;
  // akses ke Logger/EventBus/Container ditambah di stage 03/04/05
private:
  IPlatform* platform_ = nullptr;
  IBoard*    board_    = nullptr;
  BootPhase  phase_    = BootPhase::None;
  bool       shutdownRequested_ = false;
};
}
```

### `run()` loop (sketsa)

```cpp
void Runtime::run() {
  while (!shutdownRequested_) {
    uint64_t now = clock().millis();
    // serviceManager_.tickAll(now);   // diisi stage 05
    platform_->idle();                 // beri platform kesempatan I/O (stdin poll → stage 09)
  }
}
```

> `tick`/`idle` sengaja disiapkan agar stage 05 (services) & stage 09 (stdin command) tinggal menyambung.

### `main.cpp` (versi stage ini)

Masih pakai stub platform/board inline (akan diganti `SimulatorPlatform`/`SimulatorBoard` di stage 06/07). Tujuannya membuktikan boot flow jalan berurutan & mencetak fase via stdout sementara.

---

## Tasks

- [ ] Tulis header interface: `types.h`, `clock.h`, `service.h`, `platform.h`, `board.h`.
- [ ] Tulis `runtime.h` + `runtime.cpp` (boot flow + `run()` loop kosong).
- [ ] Sediakan stub `IClock`/`IPlatform`/`IBoard` minimal (di target, bukan core) untuk uji boot.
- [ ] Update `core/CMakeLists.txt` → `add_library(kairo_core ...)` dengan source nyata + `target_include_directories(... PUBLIC include)`.
- [ ] Rombak `main.cpp` memanggil boot flow, cetak transisi `BootPhase` sementara.
- [ ] Pastikan tidak ada include platform-spesifik di `core/`.

## Acceptance criteria

- `kairo-sim` build & jalan, mencetak transisi fase: `PlatformLoaded → BoardLoaded → ServicesRegistered → CoreReady → Running`, lalu shutdown bersih (mis. setelah N tick).
- `firmware/core/**` tidak meng-include header platform/OS-spesifik (cek manual / grep).
- Boot flow gagal-aman: kalau platform null saat `initCore`, error jelas (assert/log), bukan crash diam.

## How to verify

```bash
bun run build:firmware && bun run run:firmware
# output memperlihatkan urutan BootPhase dan keluar bersih
```

## Risks / notes

- `IClock` di core menjaga determinisme & testability (simulator bisa pakai fake clock). Kalau dirasa over-engineering untuk MVP, boleh sederhanakan, tapi pertahankan abstraksi waktu di platform — core jangan panggil `std::chrono` di logika service.
- Pointer mentah (`IPlatform*`) dipakai demi kesederhanaan & nanti cocok untuk lingkungan embedded tanpa heap dinamis berlebih; ownership ada di `main()`/target.
