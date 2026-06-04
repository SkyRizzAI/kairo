# 05 — Service Container & Service Manager

> Dua subsystem inti (§7): **Service Container** = Dependency Injection (register/resolve/singleton), **Service Manager** = kelola lifecycle service (Created→Running→Stopped/Failed).

- Status: ☐ Not started
- Milestone: M1 (Core Foundation)
- Depends on: 02, 03, 04
- Blocks: 06, 07, 09 (sink service-state), 10 (panel Services)

---

## Goal

- **Service Container**: registrasi & resolusi service/dependency sebagai singleton, dengan lifecycle terkelola.
- **Service Manager**: state machine lifecycle (Created, Starting, Running, Stopping, Stopped, Failed) + transisi yang ter-log & ter-publish event.
- Background service bisa di-`tick()` tiap loop.

## Scope

### In scope

- `ServiceContainer`: `registerService<T>(instance)`, `resolve<T>()`, ambil daftar service.
- `ServiceManager`: `startAll()`, `stopAll()`, `tickAll(nowMs)`, transisi state + guard.
- Emit `events::ServiceStarted` / `events::ServiceStopped` + log tiap transisi.
- Integrasi ke boot flow: `registerServices()` → `start()` memanggil `ServiceManager::startAll()`; `run()` memanggil `tickAll`.

### Out of scope

- Resolusi dependency graph otomatis / topological order kompleks. MVP: urutan registrasi = urutan start (cukup; core services didaftarkan dulu).
- Hot reload service (non-MVP).

---

## Design

### File

```text
firmware/core/include/kairo/service/
├─ service_container.h
└─ service_manager.h
firmware/core/src/service/
├─ service_container.cpp
└─ service_manager.cpp
```

### Service Container (sketsa)

```cpp
namespace kairo {
class ServiceContainer {
public:
  // simpan instance (ownership di luar atau via unique_ptr) sebagai singleton by type
  template <class T> void registerService(T* instance);
  template <class T> T*   resolve();              // nullptr kalau tak ada
  template <class T> T&   require();              // assert/fatal kalau tak ada

  // iterasi semua IService yang terdaftar (untuk Service Manager)
  const std::vector<IService*>& services() const;
private:
  std::unordered_map<std::type_index, void*> byType_;
  std::vector<IService*> services_;   // yang juga IService
};
}
```

> Implementasi by-type pakai `std::type_index`. Saat `registerService<T>`, kalau `T` turunan `IService`, dorong juga ke `services_` (urutan registrasi dipertahankan untuk start order).

### Service Manager (state machine)

```text
Created ──start()──► Starting ──ok──► Running
                         │ exception/false
                         ▼
                       Failed
Running ──stop()──► Stopping ──► Stopped
```

```cpp
namespace kairo {
class ServiceManager {
public:
  ServiceManager(ServiceContainer& c, Logger& log, EventBus& bus, IClock& clk);
  void startAll();                 // start tiap service sesuai urutan registrasi
  void stopAll();                  // reverse order
  void tickAll(uint64_t nowMs);    // panggil tick() service yang Running
  ServiceState stateOf(IService* s) const;
private:
  void transition(IService* s, ServiceState to);  // log + publish event
  // simpan map service→state
};
}
```

- `startAll`: tiap service `Created→Starting`, panggil `start()` (dibungkus try/catch), sukses → `Running` + publish `ServiceStarted`; gagal → `Failed` + log error. Kegagalan satu service **tidak** menghentikan boot (log & lanjut) — tapi catat; keputusan fatal vs lanjut didokumentasikan (MVP: lanjut, tandai Failed).
- `stopAll`: reverse, `Running→Stopping→Stopped` + publish `ServiceStopped`.
- `tickAll`: hanya service `Running` yang di-`tick(nowMs)`.

### Integrasi Runtime

- `Runtime` memegang `ServiceContainer` + `ServiceManager`.
- `registerServices()`: panggil `platform_->registerDrivers(*this)` & `board_->describeHardware(*this)` (driver/service didaftarkan ke container) — diisi penuh di stage 06/07.
- `start()` → `serviceManager.startAll()`.
- `run()` loop → `serviceManager.tickAll(clock.millis())`.
- `requestShutdown()` lalu setelah loop selesai → `serviceManager.stopAll()`.

---

## Tasks

- [ ] `ServiceContainer` (register/resolve by type + daftar IService berurutan).
- [ ] `ServiceManager` (state map, startAll/stopAll/tickAll, transition+log+event).
- [ ] Integrasi ke `Runtime`: pegang container+manager, sambungkan ke boot flow & `run()`/shutdown.
- [ ] Buat 1 service dummy sementara (mis. `HeartbeatService` yang tiap ~1000ms log "tick") untuk membuktikan start+tick+stop. (Service "beneran" menyusul stage 07.)
- [ ] Test: register→resolve mengembalikan instance sama; start menaikkan ke Running + emit event; service yang throw → Failed.

## Acceptance criteria

- Boot: `HeartbeatService` masuk `Running`, men-`tick` periodik (log "tick"), saat shutdown → `Stopped`.
- Event `ServiceStarted`/`ServiceStopped` ter-publish (terlihat via subscriber/log).
- `resolve<T>()` mengembalikan instance yang sama tiap kali (singleton).
- Service yang gagal start → state `Failed`, boot tetap lanjut, error ter-log.

## How to verify

```bash
bun run build:firmware && bun run run:firmware
# amati: ServiceStarted(Heartbeat) → beberapa "tick" → (shutdown) ServiceStopped
```

## Risks / notes

- `std::type_index` butuh RTTI (aktif default). Kalau nanti di ESP32 RTTI dimatikan, ganti ke key manual (string id). Catat sebagai follow-up; MVP host aman.
- Start order = registration order. Dependensi antar-service yang rumit di luar MVP; pastikan core services didaftarkan sebelum driver service.
