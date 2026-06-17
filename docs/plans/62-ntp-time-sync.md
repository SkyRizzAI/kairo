# 62 — NTP Time Sync

> Sync jam device ke waktu nyata via NTP setelah WiFi connect. Status bar
> langsung tampilkan waktu yang benar, bukan Unix epoch.

- Status: 🔴 Not started
- Depends on: 20 (WiFi & Networking), 16 (ESP32 Platform), 15 (Status Bar)
- Blocks: —

---

## 1. Goals

1. Deteksi WiFi connected → auto sync NTP (sekali, non-blocking)
2. Set `rt.clock().setEpochMs()` dengan hasil NTP
3. Status bar langsung tampilkan jam nyata (jam:menit)
4. Timezone offset dari ConfigStore (Plan 24)
5. Periodic resync setiap 24 jam
6. Fallback: jika no network, jam mulai dari RTC (atau epoch jika RTC invalid)

## 2. Desain

### HAL Interface (minimal — tidak perlu interface baru)

Gunakan langsung ESP-IDF SNTP client yang sudah built-in di lwIP:

```cpp
// core/src/services/ntp_service.cpp
class NtpService : public IService {
public:
    void start(Runtime& rt) override {
        rt_ = &rt;
        rt_.events().subscribe(events::NetworkConnected, [this](const Event&) {
            rt_.tasks().submit([this] { syncOnce(); }, [](auto) {});
        });
    }

    void syncOnce() {
        // ESP-IDF: esp_sntp_init() + sntp_setservername() + sntp_restart()
        // atau custom UDP socket ke pool.ntp.org:123
        // Setelah dapat: rt_.clock().setEpochMs(ntpResult);
    }
};
```

### Kenapa tidak bikin `ITimeSync` HAL?

NTP = networking concern, bukan hardware abstraction. ESP32 punya SNTP built-in
di IDF; simulator bisa inject waktu via web panel (Plan 10). Tidak perlu
interface abstrak — `NtpService` langsung panggil `IClock::setEpochMs()`.

### Trigger

- `NetworkConnected` event → submit sync ke worker thread
- Jangan di UI/app thread — blocking (UDP recv bisa 1–5 detik)
- `TaskRunner` (Plan 19.5) untuk eksekusi, hasil post balik ke main thread

### Timezone

- ConfigStore key: `time.tz_offset` (int32, menit dari UTC)
- Settings screen: tambah item "Timezone" dengan picker (atau integer input)
- Default: UTC+7 (WIB) — atau detect dari locale nanti
- Aplikasi offset di `ClockService` saat konversi epoch → jam:menit untuk display

### Periodic Resync

- Setelah sync pertama, set timer 24 jam
- Saat timer fire: cek `isOnline()`, jika ya → sync lagi
- Jangan sync lebih sering (beban server NTP publik)

### RTC Fallback

- ESP32 punya RTC internal (bertahan selama deep sleep, tidak setelah power loss)
- Saat boot: baca RTC → `setEpochMs()` → status bar tampilkan waktu terakhir
- Setelah NTP sync → update RTC + `setEpochMs()` dengan nilai akurat

## 3. Implementasi

### Fase 1 — NtpService core (1 hari)

1. Buat `firmware/core/src/services/ntp_service.cpp` + header
2. Subscribe `NetworkConnected`, submit sync ke `TaskRunner`
3. Platform-specific: panggil `rt.platform().syncNtp()` — platform implementasi
   aktual SNTP/UDP

### Platform implementations

**ESP32:**
```cpp
// platforms/esp32/src/esp32_platform.cpp
bool Esp32Platform::syncNtp() {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    // wait max 10 detik untuk sync
    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        time_t now;
        time(&now);
        rt_.clock().setEpochMs((uint64_t)now * 1000);
        return true;
    }
    return false;
}
```

**Simulator:**
```cpp
// platforms/wasm/src/wasm_platform.cpp
bool WasmPlatform::syncNtp() {
    // Simulator: ambil waktu dari host (JS Date.now() via KLP virtual cable)
    // atau inject via web panel Controls
    return false; // stub — tidak kritis untuk sim
}
```

### Fase 2 — Status Bar + Settings (0.5 hari)

1. `ClockService::getDisplayTime()` — sudah ada `status_.hour/minute`
2. Pastikan status bar refresh setelah `ClockChanged` event
3. Tambah timezone setting di `SettingsScreen` (Plan 30/60)

### Fase 3 — RTC persistence (0.5 hari)

1. ESP32: baca RTC saat boot, tulis setelah NTP sync
2. ConfigStore: simpan timestamp terakhir + drift estimate

## 4. Files

| File | Perubahan |
|------|-----------|
| `firmware/core/src/services/ntp_service.cpp` | **Baru** — NTP service |
| `firmware/core/include/nema/services/ntp_service.h` | **Baru** — header |
| `firmware/core/src/runtime.cpp` | Register NtpService ke service container |
| `firmware/platforms/esp32/src/esp32_platform.cpp` | Implementasi `syncNtp()` via SNTP |
| `firmware/platforms/wasm/src/wasm_platform.cpp` | Stub `syncNtp()` |
| `firmware/core/src/screens/settings_screen.cpp` | Item timezone |
| `firmware/core/src/services/config_store.cpp` | Key `time.tz_offset` |

## 5. Acceptance Criteria

- [ ] Setelah WiFi connect, jam status bar update ke waktu nyata (toleransi ±5 detik)
- [ ] Tanpa WiFi: jam tampilkan waktu dari RTC (jika ada) atau "--:--"
- [ ] Timezone offset bisa diubah dari Settings, refleksi langsung di status bar
- [ ] Periodic resync: jam tetap akurat setelah device nyala 48 jam
- [ ] Build hijau: host (sim) + WASM + ESP32
