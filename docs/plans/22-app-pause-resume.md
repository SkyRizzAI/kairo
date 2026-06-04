# 22 — App Pause & Resume

> User bisa **pause** app yang sedang jalan dengan hold Cancel 2 detik. State app tersimpan
> utuh (thread tetap hidup, local var terjaga). Launcher menampilkan **"Continue: \<nama app\>"**
> di baris teratas. Pilih → resume persis dari titik yang ditinggalkan.

- Status: ☐ Not started
- Milestone: M7 (UX Core)
- Depends on: **19.6 Fase C** (IApp + AppLoader + AppContext fully wired), 15 (HomeScreen)
- Blocks: —

---

## Requirement (dari user)

1. Hold **Cancel 2 detik** dari app manapun → app di-pause, layar kembali ke home launcher.
2. Launcher menampilkan **"Continue: \<namaapp\>"** sebagai item paling atas.
3. Pilih item Continue → resume app, layar kembali persis dari kondisi pause.
4. Key yang men-trigger pause (Cancel-hold) **tidak diteruskan** ke app sebagai aksi biasa.
5. Saat device restart → paused app hilang (no persistent state, keep it simple v1).
6. Hanya **satu app bisa di-pause** sekaligus.

---

## Kenapa Model Nema Ideal untuk Ini

Di model cooperative lama (plan 1–15), "pause" butuh serialisasi state manual — setiap app perlu implement `save()`/`restore()`. Mahal dan error-prone.

Di model Nema: **setiap IApp jalan di thread sendiri**. Pause = biarkan thread tetap hidup tapi masuk *idle wait loop* — tidak ada serialisasi, semua local variable & call stack terjaga otomatis oleh OS.

```
Thread ClockApp [RUNNING] → Hold Cancel 2s → Thread ClockApp [PAUSED wait loop]
                                                          ↑ tetap di heap/stack, tidak dihapus
```

Resume = keluarkan thread dari wait loop → lanjut dari baris berikutnya setelah `ctx.input().receive()`.

---

## Arsitektur

### State machine AppLoader

```
               launch(app)                  pause()
    [IDLE] ───────────────→ [APP_RUNNING] ─────────→ [APP_PAUSED + HOME]
      ↑                          ↑                         │
      │       exit()             │      resume()           │
      └──────────────────────────┴─────────────────────────┘
```

`AppLoader` mengelola dua slot:
- `foreground_` — app yang sedang aktif (dapat input)
- `paused_` — app yang di-pause (thread hidup, idle)

### Pause protocol (cooperative, transport-agnostic)

Tidak ada `vTaskSuspend` — terlalu platform-specific dan merusak portable. Sebaliknya:

1. `AppLoader::pause()` sets `appCtx_->pauseRequested_ = true`
2. `AppContext::input().receive()` — ketika pause flag aktif, receive **memblok tanpa batas** di mailbox kosong (tidak return timeout event), sehingga thread app berhenti di `receive()` secara natural
3. Saat resume: `AppLoader::resume()` clears flag → `receive()` unblock dengan data atau timeout normal → app loop lanjut

```
App thread:                       AppLoader (GUI thread):
  while (!ctx.shouldExit()) {
    InputEvent ev;
    ctx.input().receive(ev, 50)   // ← blok di sini saat pauseRequested_=true
    m.handle(ev)
    ctx.gui().present(render(m))
  }
                                  pause():   pauseRequested_=true
                                  resume():  pauseRequested_=false → mailbox signal
```

App **tidak perlu kode tambahan** — `ctx.input().receive()` yang menangani pause secara transparan. App yang sudah exist (ClockApp, CounterApp, dll) otomatis support pause tanpa diubah.

---

## Komponen yang Perlu Dibuat / Diubah

### 1. Long-press detection di `InputService`

```
firmware/core/include/kairo/services/input_service.h (modify)
firmware/core/src/services/input_service.cpp (modify)
```

Tambah tracking **per-key hold time**:

```cpp
// InputService internal:
struct KeyState {
    uint64_t pressedAtMs = 0;
    bool     held        = false;
    bool     longFired   = false;   // sudah emit LongPress, jangan ulang
};
KeyState keyState_[/* Key::Count */];

// Di poll loop atau drain:
// Saat key pressed (edge DOWN): catat pressedAtMs, held=true
// Saat tick: if held && now - pressedAtMs >= longPressMs_ && !longFired:
//    emit InputEvent{key, Edge::LongPress}
//    longFired = true
// Saat key released (edge UP): reset held, longFired
```

`InputEvent` tambah `Edge::LongPress`:

```cpp
// kairo/nema/input_event.h (modify)
enum class Edge : uint8_t { Press, Release, LongPress };
```

Threshold `longPressMs_` = 2000ms (configurable di constructor).

### 2. `AppLoader::pause()` dan `resume()`

```
firmware/core/include/kairo/app/app_loader.h (modify)
firmware/core/src/app/app_loader.cpp (modify)
```

```cpp
class AppLoader {
public:
    // ... existing launch/exit ...

    // Pause foreground app — dipanggil GuiService saat LongPress Cancel
    void pause();

    // Resume paused app — dipanggil HomeScreen saat pilih "Continue"
    void resume();

    bool        hasPaused()  const { return paused_ != nullptr; }
    const char* pausedName() const { return paused_ ? paused_->name() : nullptr; }

    // Kill paused app (sinyal exit + join) — dipanggil CloseAndOpenModal
    void killPaused();

private:
    IApp*         foreground_    = nullptr;
    nema::Thread* fgThread_      = nullptr;
    AppContext*   fgCtx_         = nullptr;

    IApp*         paused_        = nullptr;    // satu slot paused
    nema::Thread* pausedThread_  = nullptr;    // thread TETAP hidup
    AppContext*   pausedCtx_     = nullptr;
};
```

**`pause()` logic:**

```cpp
void AppLoader::pause() {
    if (!foreground_) return;
    // 1. Pindahkan ke slot paused
    paused_       = foreground_;
    pausedThread_ = fgThread_;
    pausedCtx_    = fgCtx_;
    foreground_ = nullptr; fgThread_ = nullptr; fgCtx_ = nullptr;

    // 2. Signal app masuk wait state
    pausedCtx_->setPauseRequested(true);

    // 3. Kembali ke home (GuiService: pop AppHost screen, push/activate HomeScreen)
    goHome_();   // callback ke GuiService
}
```

**`resume()` logic:**

```cpp
void AppLoader::resume() {
    if (!paused_) return;
    // 1. Pindahkan kembali ke foreground
    foreground_ = paused_;
    fgThread_   = pausedThread_;
    fgCtx_      = pausedCtx_;
    paused_ = nullptr; pausedThread_ = nullptr; pausedCtx_ = nullptr;

    // 2. Clear pause flag → app thread unblock dari receive()
    fgCtx_->setPauseRequested(false);
    fgCtx_->signalInput();   // wake receive() jika blocking

    // 3. Re-attach AppHost screen ke ViewDispatcher
    attachForeground_();   // callback ke GuiService
}
```

**Kebijakan satu slot — launch app baru saat ada paused app:**

Hanya satu app yang boleh hidup (running atau paused) sekaligus. Kalau user mencoba launch app lain sementara ada paused app → tampilkan **modal konfirmasi**:

```
┌─────────────────────────────────────┐
│  Clock is running in background.    │
│  Close it to open Counter?          │
│                                     │
│  [Close & Open]      [Cancel]       │
└─────────────────────────────────────┘
```

- **Close & Open**: kill paused app (sinyal `shouldExit`, join thread) → launch app baru.
- **Cancel**: tutup modal, tetap di home, paused app masih ada.

**Settings tidak kena policy ini** — Settings adalah `IScreen` cooperative (system screen per plan 19.6), bukan `IApp`, sehingga tidak butuh thread dan bisa dibuka kapan saja.

Modal ini ditangani `HomeScreen` via `ScreenMode::Modal` yang sudah ada di arsitektur.

### 3. `AppContext::setPauseRequested()` + `signalInput()`

```
firmware/core/include/kairo/app/app_context.h (modify)
firmware/core/src/app/app_context.cpp (modify)
```

```cpp
// AppContext (modify):
void setPauseRequested(bool v) {
    pauseRequested_.store(v, std::memory_order_release);
    if (!v) inputMailbox_.signal();  // wake thread yang lagi block
}

// AppContext::InputProxy::receive() — modifikasi internal:
bool receive(InputEvent& out, uint32_t timeoutMs) {
    while (pauseRequested_.load()) {
        // Blok dengan timeout pendek sambil polling flag
        mailbox_.receive(out, 20);   // drain spurious; ignore hasilnya
    }
    return mailbox_.receive(out, timeoutMs);
}
```

App thread berhenti di `receive()` selama `pauseRequested_ == true`. Saat `setPauseRequested(false)` dipanggil + `signal()`, receive unblock dan kembali normal.

**Memory note:** Thread paused masih di-schedule oleh FreeRTOS tapi langsung blok lagi di `mailbox_.receive(out, 20)` — konsumsi CPU ~0, hanya tidur berulang 20ms. Stack tetap dialokasi (~8KB SRAM internal).

### 4. GuiService — intercept LongPress Cancel

```
firmware/core/src/services/gui_service.cpp (modify)
```

Di GUI service input routing, sebelum dikirim ke app mailbox:

```cpp
// GuiService input drain loop:
InputEvent ev;
while (inputService_.poll(ev)) {
    // Intercept LongPress Cancel → pause
    if (ev.key == Key::Cancel && ev.edge == Edge::LongPress) {
        appLoader_.pause();
        continue;   // jangan kirim ke app
    }
    // Routing normal ke foreground app mailbox
    if (appLoader_.hasForeground())
        appLoader_.foregroundCtx().inputMailbox().send(ev);
}
```

`LongPress` Cancel **tidak pernah sampai ke app** — dibuang di routing layer. App tidak tahu bedanya.

### 5. HomeScreen — item "Continue"

```
firmware/core/src/screens/home_screen.cpp (modify)
```

HomeScreen punya akses ke `AppLoader` via `Runtime` (atau via konstruktor injection).

Di `draw()`:

```cpp
// Di awal menu list, sebelum plugin list:
if (rt_.appLoader().hasPaused()) {
    // Render baris pertama dengan highlight khusus
    char label[48];
    snprintf(label, sizeof(label), "Continue: %s", rt_.appLoader().pausedName());
    drawMenuItem(canvas, 0, label, cursor_ == 0, /* icon= */ IconResume);
    pluginOffset_ = 1;   // geser index plugin ke bawah
} else {
    pluginOffset_ = 0;
}
```

Di `update(Key k)`:

```cpp
if (k == Key::Select) {
    // Baris "Continue" → resume
    if (cursor_ == 0 && rt_.appLoader().hasPaused()) {
        rt_.appLoader().resume();
        return;
    }
    // Launch app baru — cek apakah ada paused app
    IApp* target = getAppAt(cursor_ - pluginOffset_);
    if (target && rt_.appLoader().hasPaused()) {
        // Tampilkan modal konfirmasi
        showCloseAndOpenModal(*target);   // push modal screen
        return;
    }
    if (target) rt_.appLoader().launch(*target);
}
```

**`CloseAndOpenModal`** (screen baru, `ScreenMode::Modal`):

```cpp
class CloseAndOpenModal : public IScreen {
public:
    CloseAndOpenModal(AppLoader& al, IApp& target)
        : al_(al), target_(target) {}

    ScreenMode mode() const override { return ScreenMode::Modal; }

    void draw(Canvas& c) override {
        // Teks: "<paused_name> is running in background.\nClose it to open <target_name>?"
        // Dua item: [Close & Open] / [Cancel], highlight sesuai cursor_
    }

    void update(Key k) override {
        if (k == Key::Left || k == Key::Right) cursor_ ^= 1;
        if (k == Key::Select) {
            if (cursor_ == 0) {           // Close & Open
                al_.killPaused();         // sinyal exit + join thread paused
                al_.launch(target_);
            }
            // cursor_==1 (Cancel) atau Cancel key → tutup modal saja
            rt_->view().pop();
        }
        if (k == Key::Cancel) rt_->view().pop();
    }

private:
    AppLoader& al_;
    IApp&      target_;
    int        cursor_ = 0;
};
```

---

## Urutan Implementasi

```
1. InputEvent::Edge::LongPress + InputService hold tracking
   → Test: simulator, hold Cancel 2s → log "LongPress:Cancel"

2. AppContext::setPauseRequested() + modifikasi receive()
   → Test: unit test host — pause flag set → receive() blok; clear → resume

3. AppLoader::pause() + resume() + satu-slot policy
   → Test: simulator — launch ClockApp, pause, status AppLoader = hasPaused()

4. GuiService: intercept LongPress Cancel → appLoader_.pause()
   → Test: simulator — hold Cancel di ClockApp → kembali ke home

5. HomeScreen: render "Continue: Clock" + Select → resume
   → Test: simulator — item muncul, pilih → ClockApp resume dari posisi terakhir

6. Flash ke dev board + verify end-to-end
   → Hardware: hold Cancel di CounterApp (counter = 7), kembali home,
     launch apps lain (navigasi tetap jalan), Continue: Counter → counter masih 7
```

---

## Acceptance Criteria

- [ ] Hold Cancel < 2s di app → tidak trigger pause (normal back navigation jika ada)
- [ ] Hold Cancel ≥ 2s di app → kembali ke HomeScreen, item "Continue: \<nama\>" muncul di atas
- [ ] Input ke app terhenti total saat paused (tidak ada spurious key masuk ke app)
- [ ] App thread tetap hidup tapi CPU ~0 (verifikasi via log / high-water-mark stack)
- [ ] Select "Continue" → app lanjut dari state tepat di titik pause (contoh: counter tidak reset)
- [ ] Launch app baru saat ada paused app → modal konfirmasi muncul
- [ ] Modal "Close & Open" → paused app di-kill, app baru launch
- [ ] Modal "Cancel" → home tetap, paused app tetap ada
- [ ] Settings bisa dibuka kapan saja tanpa modal (system screen, bukan IApp)
- [ ] App yang tidak di-pause tetap tidak terpengaruh
- [ ] Simulator (host) dan ESP32 perilaku identik

---

## Anggaran Memori

| Item | Ukuran | Lokasi |
|---|---|---|
| Stack thread paused app | ~8 KB | Internal SRAM |
| Heap app (model, buffer) | ~16–32 KB | PSRAM |
| `AppLoader` pause slot | pointer only | Internal SRAM |

Satu paused app: **~24–40 KB total**. Di PSRAM 8MB+SRAM 512KB: aman. Tapi tetap catat high-water-mark stack saat dev (`uxTaskGetStackHighWaterMark`).

---

## Non-Goals (v1)

- **Persistent pause**: state tidak disimpan ke NVS/SPIFFS — paused app hilang saat restart.
- **Multi-app**: hanya satu app boleh hidup (running/paused) sekaligus. Launch app lain = modal dulu.
- **Cancel short-press sebagai back**: long-press vs short-press Cancel untuk navigasi normal adalah urusan masing-masing app, bukan pause system.
- **Background task**: paused app tidak melanjutkan task-nya (TaskRunner job, download). Thread hanya idle. Jika app butuh background task, itu scope plan terpisah (background service model).
- **vTaskSuspend**: sengaja tidak dipakai — terlalu platform-specific, tidak portable ke simulator, dan tidak diperlukan karena cooperative pause bekerja.
