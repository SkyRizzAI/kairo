# 64 — App Crash Recovery

> Auto-deteksi + restart app yang crash/hang. Saat ini JS app crash = app
> thread mati, user harus manual navigasi ke launcher dan buka ulang.

- Status: 🔴 Not started
- Depends on: 19.6 (App Model), 58 (JS Runtime), 46 (Process Monitor)
- Blocks: —

---

## 1. Goals

1. Deteksi app thread abnormal termination (exception, deadlock, exit non-zero)
2. Auto-restart dengan backoff policy (1x → langsung, 2x → 5s delay, 3x → 30s
   delay, 4x → mark FAILED)
3. User notification: toast "App X crashed — restarting..."
4. Persistent crash counter: setelah 3 crash dalam 60 detik → berhenti restart
5. CLI `ps` tampilkan crash count + status (running / crashed / failed)

## 2. Arsitektur

### Crash Detection

Di `AppHost::threadEntry()`:

```cpp
// core/src/app/app_host.cpp
void AppHost::threadEntry(void* self) {
    auto* h = static_cast<AppHost*>(self);
    int exitCode = 0;
    try {
        h->app_.run(*h);
    } catch (const std::exception& e) {
        h->rt_.log().error("AppHost", "app crashed",
            {{"app", h->app_.name()}, {"what", e.what()}});
        exitCode = 1;
    } catch (...) {
        h->rt_.log().error("AppHost", "app crashed (unknown)",
            {{"app", h->app_.name()}});
        exitCode = 1;
    }
    // Thread exit → signal AppHostManager via AppHostExit event
    h->rt_.events().emit(events::AppHostExited{
        .appId = h->app_.id(),
        .exitCode = exitCode
    });
}
```

### Restart Policy — AppHostManager

```cpp
// core/src/app/app_host_manager.cpp
struct CrashRecord {
    uint32_t crashCount;
    uint64_t firstCrashMs;
    uint64_t lastCrashMs;
};

void AppHostManager::onAppExited(const AppHostExitedEvent& e) {
    if (e.exitCode == 0) return; // clean exit, tidak restart

    auto& rec = crashRecords_[e.appId];
    auto now = rt_.clock().epochMs();

    // Reset counter jika crash terakhir >60 detik yang lalu
    if (now - rec.firstCrashMs > 60000) {
        rec.crashCount = 0;
        rec.firstCrashMs = now;
    }
    rec.crashCount++;
    rec.lastCrashMs = now;

    if (rec.crashCount >= 4) {
        rt_.log().error("AppHostManager", "app marked FAILED",
            {{"app", e.appId}, {"crashes", rec.crashCount}});
        report_->markFailed(e.appId);
        // toast: "App X has crashed repeatedly — disabled"
        return;
    }

    uint32_t delayMs = 0;
    if (rec.crashCount == 2) delayMs = 5000;
    if (rec.crashCount == 3) delayMs = 30000;

    rt_.log().warn("AppHostManager", "scheduling restart",
        {{"app", e.appId}, {"attempt", rec.crashCount}, {"delayMs", delayMs}});

    // toast: "App X crashed — restarting..."
    scheduleRestart(e.appId, delayMs);
}
```

### User Notification

Gunakan pattern yang sudah ada — `NotificationManager` atau toast via
StatusBar/overlay:

```cpp
// toast muncul 3 detik, hilang otomatis
rt_.notifications().toast("Counter crashed — restarting...", 3000);
```

### CLI Integration (Plan 46 Process Monitor)

```bash
> ps
PID   APP                        RUNTIME  TIER  CRASH  STATUS
1     com.palanu.counter         js       app   0/3    running
2     com.palanu.sysinfo         js       app   4/3    FAILED
```

### Non-Goal

- **Memory sandboxing.** ESP32-S3 tidak punya MMU. Crash isolation hanya via
  thread — satu app tidak bisa corrupt memory app lain (best-effort, tidak
  dijamin hardware).
- **JS error boundary.** QuickJS tidak punya `try/catch` di level engine host.
  Unhandled JS exception = thread exit. Kita tangkap di C++, bukan di JS.

## 3. Implementasi

### Fase 1 — Crash Detection (0.5 hari)

1. Wrap `app_.run()` di `threadEntry()` dengan try/catch
2. Emit `AppHostExited` event dengan exit code
3. Handle clean exit (`process.exit(0)` dari JS, atau app return normal) —
   tidak restart

### Fase 2 — Restart Policy (1 hari)

1. Buat `AppHostManager` — track crash records per app ID
2. Implementasi backoff: 0s → 5s → 30s → FAILED
3. Reset counter setelah 60 detik crash-free
4. Toast notification via `NotificationManager`

### Fase 3 — CLI + Process Monitor (0.5 hari)

1. Update Plan 46 `ps` output: tambah kolom CRASH + STATUS
2. `app info <id>` tampilkan crash history

## 4. Files

| File | Perubahan |
|------|-----------|
| `firmware/core/src/app/app_host.cpp` | Wrap `app_.run()` try/catch, emit event |
| `firmware/core/src/app/app_host_manager.cpp` | **Baru** — restart policy |
| `firmware/core/include/nema/app/app_host_manager.h` | **Baru** — header |
| `firmware/core/include/nema/events/core_events.h` | Tambah `AppHostExited` event |
| `firmware/core/src/services/notification_manager.cpp` | Tambah `toast()` method |
| `firmware/core/src/services/process_monitor.cpp` | Crash count + status |

## 5. Acceptance Criteria

- [ ] JS app dengan `throw new Error("test")` → thread mati, toast "crashed —
  restarting...", app restart otomatis
- [ ] Restart backoff: 1st immediate, 2nd 5s, 3rd 30s, 4th → FAILED (tidak
  restart lagi)
- [ ] Clean exit (`process.exit(0)`) tidak trigger restart
- [ ] Crash counter reset setelah app berjalan >60 detik tanpa crash
- [ ] `ps` CLI tampilkan crash count + status (running / crashed / FAILED)
- [ ] App yang marked FAILED tidak bisa di-launch lagi (launcher disabled / grey)
- [ ] Build hijau: host + WASM + ESP32
