# 21 — Screen Sleep & Lock Screen

> Sistem dua-tahap: **layar mati otomatis** setelah inaktif, lalu **lock state** kalau dibiarkan
> lebih lama. Key yang menekan saat layar mati hanya membangunkan layar — tidak diteruskan
> ke app. Berlaku identik di simulator dan hardware e-ink.

- Status: ✅ Done
- Milestone: M7 (UX Polish)
- Depends on: 19.5 (Nema kernel), 19.6 (app model / GuiService) — **direvisi dari versi awal**

> **Catatan revisi:** Plan awal dirancang sebelum 19.5/19.6. Komponen yang berubah:
> `DisplayPowerManager` sekarang tinggal di **GuiService** (bukan Runtime) karena GuiService
> adalah satu-satunya owner ViewDispatcher + render loop. `Runtime::deliverKey()` tidak jadi
> dibuat (jalur key sudah: InputService → GuiService drain). Board driver tidak berubah
> (sudah direfactor ke InputService oleh 19.5).

---

## Requirements

1. **Screen Sleep** — setelah 15 detik tanpa aktivitas → layar blank (putih di e-ink, kosong di sim).
2. **Auto Lock** — setelah layar sleep 15 detik tanpa aktivitas → state berubah ke LOCKED.
3. **Wake key** — semua key saat SLEEP → bangunkan layar, **key itu dibuang** (tidak sampai ke app).
4. **Lock screen** — semua key saat LOCKED → tampilkan layar lock, key tetap dibuang kecuali
   unlock gesture (Select × 2).
5. **Lock screen UI** — layar putih polos, pojok kiri bawah: `"Press Select 2x to unlock"`.
6. **Unlock** — tekan Select dua kali → kembali ke state ACTIVE, resume screen sebelumnya.

**Real case yang harus aman:** user sedang di app, layar sleep, user pencet Select → layar
bangun, highlight cursor di app tetap di posisi sebelumnya. Key Select tidak dikonsumsi app.

---

## State machine

```
         no key 15s            no key 15s
ACTIVE ──────────────► SLEEP ──────────────► LOCKED
  ▲                      │                      │
  │      any key          │                      │
  └───────────────────────┘                      │
  ▲                                              │
  └─────── Select×2 (via LockScreen) ───────────┘
```

| State    | Layar      | Key behavior                           | Render |
|----------|------------|----------------------------------------|--------|
| ACTIVE   | normal     | diteruskan ke app                      | ya     |
| SLEEP    | putih/blank| **dibuang**, layar bangun (→ ACTIVE)   | tidak  |
| LOCKED   | LockScreen | dibuang kecuali Select×2 (via screen)  | ya     |

---

## Arsitektur (Nema-aligned)

Semua logika DPM tinggal di dalam **GuiService** — satu-satunya thread yang boleh menyentuh
ViewDispatcher + Canvas. Tidak ada perubahan di Runtime, tidak ada `deliverKey()` di Runtime.

```
GuiService::loop() [nema_gui thread]
    ├─ drain InputService
    │       └─→ dpm_.deliverKey(key, now)   ← interception point
    │              ├─ ACTIVE : update lastActivity, return false → vd.handleKey()
    │              ├─ SLEEP  : wake(), return true (key dibuang)
    │              └─ LOCKED : vd.handleKey() → LockScreen, return true
    │
    ├─ dpm_.tick(now)                        ← state machine timeout
    │
    ├─ refreshStatus / vd.tick()
    │
    └─ render
           ├─ sleeping + just-entered-sleep → flush blank frame once (e-ink safe)
           └─ not sleeping + redraw pending → renderOnce()
```

---

## Komponen baru

### 1. `DisplayPowerManager`

```
firmware/core/include/palanu/services/display_power_manager.h
firmware/core/src/services/display_power_manager.cpp
```

```cpp
enum class DPMState { Active, Sleep, Locked };

class DisplayPowerManager {
public:
    void init(ViewDispatcher& vd, IDisplayDriver* display, IClock& clock,
              LockScreen& lockScreen,
              uint64_t sleepMs = 15000, uint64_t lockMs = 15000);

    // Returns true = key consumed (caller must NOT forward to vd.handleKey)
    bool deliverKey(Key k, uint64_t nowMs);

    void tick(uint64_t nowMs);

    // Called by LockScreen when Select×2 — must be called from GuiService thread
    void unlock();

    bool isActive()        const;
    bool isSleeping()      const;
    bool isLocked()        const;
    bool takeEnteredSleep(); // one-shot: true the first frame after sleep entry

private:
    void enterSleep();
    void enterLocked();
    void wake(uint64_t nowMs);

    DPMState       state_             = DPMState::Active;
    uint64_t       lastActivityMs_    = 0;
    uint64_t       sleepEnterMs_      = 0;
    uint64_t       sleepTimeoutMs_    = 15000;
    uint64_t       lockTimeoutMs_     = 15000;
    bool           enteredSleepFlag_  = false;

    ViewDispatcher*  vd_          = nullptr;
    IDisplayDriver*  display_     = nullptr;
    IClock*          clock_       = nullptr;
    LockScreen*      lockScreen_  = nullptr;
};
```

**deliverKey():**
- ACTIVE → update `lastActivityMs_`, return false (key forwarded)
- SLEEP  → `wake(now)`, return true (key dibuang)
- LOCKED → `vd_->handleKey(k)` (LockScreen di top of stack menerima key), return true

**tick():**
- ACTIVE + timeout → `enterSleep()`
- SLEEP + timeout → `enterLocked()`

**enterSleep():**
- Set state, set `enteredSleepFlag_`
- `display_->sleep()` — sinyal ke frontend (SimDisplay: kirim `display_sleep` event)

**enterLocked():**
- Set state, `vd_->push(*lockScreen_)`, `vd_->requestRedraw()`

**wake():**
- Set state ACTIVE, update `lastActivityMs_`
- `display_->wake()` — sinyal ke frontend (`display_wake`)
- `vd_->requestRedraw()`

**unlock():**
- `vd_->pop()` (lepas LockScreen), set state ACTIVE
- `lastActivityMs_ = clock_->millis()`
- `vd_->requestRedraw()`

---

### 2. `LockScreen`

```
firmware/core/include/palanu/screens/lock_screen.h
firmware/core/src/screens/lock_screen.cpp
```

Cooperative `IScreen` — tidak butuh thread sendiri. Tampil di atas screen stack saat LOCKED.

```cpp
class LockScreen : public IScreen {
public:
    void setDpm(DisplayPowerManager& dpm);

    ScreenMode mode() const override { return ScreenMode::Fullscreen; }
    void enter() override;           // reset selectCount_
    void update(Key k) override;     // Select×2 → dpm_->unlock()
    void draw(Canvas& canvas) override; // blank white + text pojok kiri bawah
private:
    DisplayPowerManager* dpm_        = nullptr;
    int                  selectCount_ = 0;
};
```

---

### 3. `IDisplayDriver` — tambah `sleep()` dan `wake()`

```cpp
// hal/display.h — default no-op
virtual void sleep() {}
virtual void wake()  {}
```

**SimDisplay** — override:
```cpp
void sleep() override { simEmit(R"({"type":"display_sleep"})"); }
void wake()  override { simEmit(R"({"type":"display_wake"})");  }
```

**EinkDisplay** — default no-op cukup:
- `sleep()`: no-op — e-ink menahan frame terakhir (putih) tanpa power
- `wake()`: no-op — redraw normal dijadwalkan oleh `wake()`

---

### 4. Perubahan `GuiService`

**Header** — tambah member baru:
```cpp
LockScreen          lockScreen_;
DisplayPowerManager dpm_;
IDisplayDriver*     display_ = nullptr;
```

**`start()`** — init DPM sebelum spawn thread:
```cpp
display_ = rt_.container().resolve<IDisplayDriver>();
dpm_.init(rt_.view(), display_, rt_.clock(), lockScreen_);
lockScreen_.setDpm(dpm_);
thread_.start(...);
```

**`loop()`** — 5 perubahan:
1. `now` dihitung di awal loop (bukan setelah input drain)
2. Input drain memanggil `dpm_.deliverKey()` sebelum `vd.handleKey()`
3. `dpm_.tick(now)` dipanggil setelah input
4. Render skip saat `isSleeping()`
5. Blank frame flush saat `takeEnteredSleep()`

---

### 5. Simulator frontend — visual feedback sleep

**`useSimSocket.ts`** — tambah `displaySleeping: boolean` ke `SimState`:
```ts
if (type === "display_sleep") return { ...prev, displaySleeping: true };
if (type === "display_wake")  return { ...prev, displaySleeping: false };
```

**`DisplayPanel.tsx`** — overlay gelap saat `displaySleeping`:
```tsx
{displaySleeping && (
  <div style={sleepOverlayStyle}>DISPLAY SLEEP</div>
)}
```

---

## Urutan implementasi

1. Tambah `IDisplayDriver::sleep()` / `wake()` (default no-op)
2. Buat `LockScreen` (header + impl)
3. Buat `DisplayPowerManager` (header + impl)
4. Update `GuiService` header + impl — integrasi DPM
5. Override `sleep()`/`wake()` di `SimDisplay`
6. Update `CMakeLists.txt` — tambah 2 source baru
7. Update frontend (`useSimSocket.ts` + `DisplayPanel.tsx`)
8. Test simulator: biarkan idle 15s → blank; 30s → lock; wake; unlock
9. Flash ke dev board — verifikasi di e-ink

---

## Acceptance criteria

- [ ] Idle 15s → layar putih (e-ink menahan frame putih, simulator canvas blank + overlay SLEEP)
- [ ] Key saat SLEEP → layar kembali dengan konten asli, **key tidak mempengaruhi app**
- [ ] Idle 30s total (15+15) → LockScreen muncul di atas apapun
- [ ] Tekan Select 1× di lock → tidak unlock, counter reset kalau non-Select ditekan duluan
- [ ] Tekan Select 2× berturut → kembali ke screen sebelumnya, state ACTIVE
- [ ] Services dan plugin `tick()` tetap jalan saat SLEEP (clock tidak berhenti)
- [ ] Setelah wake, status bar tetap update normal
- [ ] Tidak ada full refresh ekstra / black flash di e-ink saat wake dari sleep

---

## Scope & batasan

**In scope:**
- Timer berbasis `clock().millis()` (bukan RTC)
- Timeout hardcoded 15s+15s (via constructor, bukan NVS)
- Lock screen statis (Select×2, tanpa PIN/password)

**Out of scope:**
- PIN/passcode entry
- NVS persistent lock settings
- Battery-aware sleep
- Wake dari deep sleep ESP32 (interrupt hardware)
