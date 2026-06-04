# 24 — Config Store (Persistent Key-Value)

> Interface tunggal `IConfigStore` (namespace + key → string/int64) dengan dua implementasi:
> **NVS** di ESP32 (internal flash, 24 KB, thread-safe, wear-leveled) dan
> **MemConfigStore** di simulator (in-memory). Tidak butuh microSD.

- Status: ✅ Done
- Milestone: M7 (UX Polish)
- Depends on: 19.5 (Runtime + ServiceContainer pattern)

---

## Desain

```
IConfigStore  (core — hardware-agnostic)
├── NvsConfigStore   (boards/dev-board — ESP-IDF NVS)
└── MemConfigStore   (platforms/simulator — std::map)
```

Diregistrasi di container sama seperti driver lain:
```cpp
rt.container().registerService(&config_);
rt.container().registerAs<IConfigStore>(&config_);
```

Diakses via `rt.config()`.

---

## API

```cpp
// Read — false jika key tidak ada, out tidak diubah
bool getString(const char* ns, const char* key, std::string& out) const;
bool getInt   (const char* ns, const char* key, int64_t& out)     const;

// Read dengan default
std::string getString(const char* ns, const char* key, const char* def) const;
int64_t     getInt   (const char* ns, const char* key, int64_t    def)  const;

// Write (langsung commit)
void setString(const char* ns, const char* key, const std::string& val);
void setInt   (const char* ns, const char* key, int64_t val);

// Delete
bool remove(const char* ns, const char* key);
```

Constraint NVS: namespace ≤ 15 char, key ≤ 15 char.

---

## Namespace conventions

| Namespace | Key             | Tipe   | Dipakai oleh         |
|-----------|-----------------|--------|----------------------|
| `dpm`     | `sleep_ms`      | int64  | DisplayPowerManager  |
| `dpm`     | `lock_ms`       | int64  | DisplayPowerManager  |
| `wifi`    | `ssid`          | string | WifiApp (nanti)      |
| `wifi`    | `password`      | string | WifiApp (nanti)      |

---

## Integrasi DPM

`GuiService::start()` membaca `dpm/sleep_ms` dan `dpm/lock_ms` sebelum spawn thread.
`SleepSettingsScreen` menyimpan ke config setiap kali nilai diubah (langsung commit).

---

## Acceptance criteria

- [ ] Sleep/lock timeout tersimpan di NVS, survive reboot
- [ ] Simulator: nilai persist selama sesi (in-memory)
- [ ] `rt.config().setInt("dpm","sleep_ms",30000)` → reboot → `getInt("dpm","sleep_ms",15000)` returns 30000
- [ ] Key tidak ada → default value dikembalikan
- [ ] Simulator + ESP32 builds clean
