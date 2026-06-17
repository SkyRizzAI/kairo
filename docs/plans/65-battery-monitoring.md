# 65 — Battery Monitoring (SkyRizz E32)

> Driver monitoring baterai untuk SkyRizz E32. Status bar icon battery +
> persentase. Jika hardware tidak punya sirkuit battery sense, fallback ke
> dummy driver.

- Status: 🔴 Not started
- Depends on: 28 (SkyRizz E32), 16 (ESP32 Platform)
- Blocks: —

---

## 1. Goals

1. Baca tegangan baterai via ADC (jika tersedia di SkyRizz E32)
2. Konversi tegangan → persentase (0–100%)
3. Deteksi charging (USB VBUS atau dedicated pin)
4. Emit `BatteryChanged` event → status bar icon update
5. Fallback: jika tidak ada hardware battery sense, dummy driver return 85%

## 2. Hardware Audit (SkyRizz E32)

**Sebelum mulai implementasi, cek schematic / board_config.h:**

| Yang dicari | Keterangan |
|-------------|-----------|
| ADC pin untuk battery voltage | GPIO mana? Voltage divider ratio? |
| Referensi tegangan ADC | 3.3V? Atau internal reference? |
| Pin charging detect | USB VBUS? Atau dedicated GPIO dari charger IC? |
| Fuel gauge IC | Ada? (MAX17048, BQ27441, dsb.) |

**Jika SkyRizz E32 tidak punya battery sense circuit:**
→ Implementasi `DummyBatteryDriver` yang return hardcoded 85% + "not charging".
Icon tetap muncul di status bar — lebih baik dari blank.

## 3. Arsitektur

### HAL Interface (sudah ada skeleton di Plan 16)

```cpp
// hal/battery.h
enum class BatteryState { Unknown, Discharging, Charging, Full };

struct BatteryInfo {
    uint8_t percentage;      // 0-100
    BatteryState state;
    uint32_t voltageMv;      // optional, 0 jika tidak tersedia
};

class IBatteryDriver {
public:
    virtual BatteryInfo read() = 0;
};
```

### Esp32BatteryDriver

```cpp
// platforms/esp32/src/esp32_battery_driver.cpp
class Esp32BatteryDriver : public IBatteryDriver, public IService {
public:
    void start(Runtime& rt) override {
        rt_ = &rt;
        adcChannel_ = rt_.config().getInt("battery.adc_channel", -1);
        if (adcChannel_ < 0) {
            rt_.log().warn("Battery", "no ADC channel configured, using dummy");
            return;
        }
        // Start periodic poll
        pollTimer_ = rt_.timers().create(1000, [this] { poll(); });
    }

    BatteryInfo read() override { return lastInfo_; }

private:
    void poll() {
        // ESP-IDF: adc1_get_raw(adcChannel_)
        // Konversi raw → voltage → percentage (linear map 3.2V-4.2V)
        // Emit BatteryChanged event jika berubah signifikan (≥2%)
    }
};
```

### Voltage → Percentage Mapping (LiPo/Li-ion)

```
4.2V = 100%
4.0V = 80%
3.8V = 60%
3.7V = 40%
3.6V = 20%
3.5V = 10%
3.2V = 0%  (cutoff)
```

Map linear dengan clamp atau lookup table sederhana.

### Charging Detection

- **Via USB VBUS:** ESP32-S3 USB OTG punya `usb_detect` — jika VBUS 5V,
  berarti plugged in.
- **Via dedicated GPIO:** jika board punya charger IC dengan status pin (misal
  `CHG_STAT`), baca GPIO.
- **Fallback:** asumsikan "discharging" jika tidak ada deteksi.

### Dummy driver (fallback)

```cpp
class DummyBatteryDriver : public IBatteryDriver {
public:
    BatteryInfo read() override {
        return { .percentage = 85, .state = BatteryState::Discharging, .voltageMv = 0 };
    }
};
```

## 4. Status Bar Integration

Sudah ada icon battery di Plan 53/60:
- `status.battery` — ikon baterai dengan level bar (4 level: 0-25-50-75-100%)
- `status.charging` — ikon petir overlay saat charging

Update flow:
1. `BatteryDriver::poll()` → emit `BatteryChanged{percentage, state}`
2. `StatusBar` subscribe event → update icon + teks persentase
3. Render di `StatusBar::draw()` setiap frame

## 5. Settings Screen

Tambah item "Battery" di Settings:
- Persentase + tegangan (jika tersedia)
- Status charging/discharging
- (Future: battery health, cycle count — jika fuel gauge IC)

## 6. Implementasi

### Fase 1 — Hardware Audit (0.5 hari)

1. Cek schematic SkyRizz E32 — apakah ada battery ADC?
2. Jika ya: tentukan GPIO, voltage divider ratio, referensi ADC
3. Jika tidak: siapkan dummy driver

### Fase 2 — Driver Implementasi (1 hari)

1. Implementasi `Esp32BatteryDriver` (jika HW tersedia)
2. Atau implementasi `DummyBatteryDriver` + dokumentasi bahwa baterai
   monitoring butuh HW rev
3. Register ke service container
4. Tambah ConfigStore key: `battery.adc_channel`, `battery.voltage_divider`

### Fase 3 — UI Integration (0.5 hari)

1. Status bar icon battery (4 level) + charging overlay
2. Settings page battery info
3. (Optional) DPM: sleep device saat battery ≤ 5%

## 7. Files

| File | Perubahan |
|------|-----------|
| `firmware/platforms/esp32/src/esp32_battery_driver.cpp` | **Baru** — implementasi driver |
| `firmware/platforms/esp32/include/nema/esp32/esp32_battery_driver.h` | **Baru** — header |
| `firmware/core/include/nema/hal/battery.h` | **Baru** — IBatteryDriver interface |
| `firmware/core/src/services/dummy_battery_driver.cpp` | **Baru** — fallback dummy |
| `firmware/core/src/ui/widgets/status_bar.cpp` | Battery icon + percentage |
| `firmware/core/src/screens/settings_screen.cpp` | Battery info item |
| `firmware/boards/skyrizz-e32/board_config.h` | Konstanta battery (ADC pin, divider) |

## 8. Acceptance Criteria

- [ ] Jika HW punya battery ADC: persentase akurat (±5%) dibanding multimeter
- [ ] Charging detection berfungsi (icon berubah saat USB dicolok/cabut)
- [ ] Battery icon di status bar update ≤2 detik setelah perubahan
- [ ] Jika HW tidak punya: dummy driver tampilkan ikon battery 85% (bukan blank)
- [ ] `BatteryChanged` event ter-emit untuk subscriber lain (DPM, logging)
- [ ] Build hijau: host + WASM + ESP32
