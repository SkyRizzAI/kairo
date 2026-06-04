# 04 — Event Bus

> Backbone komunikasi internal (§7 master plan). Subsystem publish event, subscriber bereaksi — decoupled.

- Status: ☐ Not started
- Milestone: M1 (Core Foundation)
- Depends on: 02 (boleh paralel dengan 03)
- Blocks: 05, 06, 07, 09 (sink event), 10 (panel Events)

---

## Goal

- Event bus in-process publish/subscribe.
- Event punya `name` (string stabil) + payload key/value opsional.
- Subscribe by event name & wildcard `"*"` (untuk bridge yang mau dengar semua → stream ke web).
- Daftar event standar terdefinisi sebagai konstanta (master plan §7).

## Scope

### In scope

- `Event` struct + konstanta nama event standar.
- `EventBus`: `publish(event)`, `subscribe(name, handler) → SubscriptionId`, `unsubscribe(id)`.
- Dispatch **synchronous** (cukup untuk MVP single-thread loop).
- Integrasi ke `Runtime` (`EventBus& events()`), publish `SystemBoot`/`SystemReady` di boot flow.

### Out of scope

- Async/queued dispatch lintas thread (single-thread loop MVP).
- Event persistence / replay.

---

## Design

### File

```text
firmware/core/include/kairo/event/
├─ event.h        # Event, EventName konstanta
└─ event_bus.h    # EventBus
firmware/core/src/event/
└─ event_bus.cpp
```

### Sketsa

```cpp
// event.h
namespace kairo {
struct Event {
  const char* name;                       // gunakan konstanta di bawah
  std::vector<Field> payload;             // reuse Field dari log_entry.h (key/value)
};

namespace events {
  inline constexpr const char* SystemBoot           = "SystemBoot";
  inline constexpr const char* SystemReady          = "SystemReady";
  inline constexpr const char* ServiceStarted       = "ServiceStarted";
  inline constexpr const char* ServiceStopped       = "ServiceStopped";
  inline constexpr const char* PluginLoaded         = "PluginLoaded";     // (non-MVP, deklarasi saja)
  inline constexpr const char* PluginUnloaded       = "PluginUnloaded";
  inline constexpr const char* NotificationCreated  = "NotificationCreated";
  inline constexpr const char* BatteryChanged       = "BatteryChanged";
  inline constexpr const char* NetworkConnected     = "NetworkConnected";
  inline constexpr const char* NetworkDisconnected  = "NetworkDisconnected";
}
}

// event_bus.h
namespace kairo {
using EventHandler = std::function<void(const Event&)>;
using SubscriptionId = uint32_t;

class EventBus {
public:
  SubscriptionId subscribe(const char* name, EventHandler h); // name "*" = semua
  void unsubscribe(SubscriptionId id);
  void publish(const Event& e);
private:
  struct Sub { SubscriptionId id; std::string name; EventHandler handler; };
  std::vector<Sub> subs_;
  SubscriptionId nextId_ = 1;
};
}
```

### Perilaku

- `publish` memanggil handler yang `name` cocok **atau** subscribe `"*"`.
- Handler dipanggil sinkron pada thread pemanggil (loop utama).
- Aman bila handler publish event lagi (re-entrancy) — iterasi atas salinan daftar sub, atau guard sederhana. Dokumentasikan batasannya.

### Integrasi

- `Runtime::events()` mengembalikan `EventBus&`.
- Boot flow:
  - awal boot → `publish({events::SystemBoot})`
  - setelah `start()` sukses → `publish({events::SystemReady})`
- Setiap publish juga sebaiknya di-`log().debug("EventBus", name)` agar terlihat di Logs (membantu observability).

---

## Tasks

- [ ] `event.h` (struct + konstanta nama event) reuse `Field`.
- [ ] `event_bus.h` + `event_bus.cpp` (subscribe/unsubscribe/publish, wildcard).
- [ ] Tambah `EventBus& events()` ke `Runtime`.
- [ ] Publish `SystemBoot` & `SystemReady` di boot flow + log tiap publish.
- [ ] Test kecil: subscribe spesifik & wildcard menerima event yang benar; unsubscribe bekerja.

## Acceptance criteria

- Subscriber `"*"` menerima `SystemBoot` lalu `SystemReady` saat boot.
- Subscriber `"BatteryChanged"` hanya menerima event itu.
- `unsubscribe` menghentikan delivery.
- Re-entrant publish tidak meng-corrupt iterasi (tidak crash).

## How to verify

```bash
bun run build:firmware && bun run run:firmware
# di log akan terlihat publish SystemBoot/SystemReady (via log().debug pada publish)
```

## Risks / notes

- `std::function` ada overhead alokasi; untuk MVP host tidak masalah. Catat untuk audit nanti di hardware.
- Nama event sebagai `const char*` konstan → murah & stabil; payload pakai `Field` agar gampang diserialisasi ke JSON di stage 09.
