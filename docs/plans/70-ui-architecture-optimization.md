# 70 — UI Architecture Optimization: FPS, Screen Stack, App Isolation, Animation & Font Manager

> **Analisa + Planning** — BELUM implementasi.
> Fokus: arsitektur rendering agar FPS naik, screen stack ala Android, app isolation,
> modal/popup system, frame-based animation library, dan font manager.

---

## DAFTAR ISI

1. [Audit Saat Ini: Rendering Pipeline & FPS Bottleneck](#1-audit-saat-ini)
2. [Audit Saat Ini: Screen Stack & Navigasi](#2-audit-saat-ini-screen-stack)
3. [Audit Saat Ini: App Isolation](#3-audit-saat-ini-app-isolation)
4. [Audit Saat Ini: Modal / Popup System](#4-audit-saat-ini-modal-popup)
5. [Audit Saat Ini: Animation System](#5-audit-saat-ini-animation)
6. [Audit Saat Ini: Font System](#6-audit-saat-ini-font)
7. [Plan: FPS Optimization](#7-plan-fps-optimization)
8. [Plan: Android-Style Screen Stack](#8-plan-screen-stack)
9. [Plan: Window Isolation for Apps](#9-plan-window-isolation)
10. [Plan: Modal / Popup / Toast System](#10-plan-modal-popup-toast)
11. [Plan: Frame-Based Animation Library](#11-plan-animation)
12. [Plan: Font Manager](#12-plan-font-manager)
13. [Phase Execution Order](#13-phase-execution)

---

## 1. Audit Saat Ini: Rendering Pipeline & FPS Bottleneck

### Current Render Loop (GuiService::loop, gui_service.cpp:141)

```
Loop setiap ~5-15ms:
  1. Drain InputService → DPM intercept → ViewDispatcher::handleAction/code
  2. DPM::tick (sleep/lock state machine)
  3. refreshStatus (clock/wifi — hanya tiap 10 detik)
  4. ViewDispatcher::tick (screen.tick())
  5. TaskRunner::drainCompletions
  6. IF redrawPending AND NOT sleeping:
       setRenderTick(now)
       restore server theme + scale
       server_->renderFrame(canvas, vd, status)
         → canvas.clear()
         → StatusBar.draw()
         → screen.draw(canvas)
         → FPS overlay (optional)
         → canvas.flush() → display driver
```

### FPS Bottlenecks Identified

| Bottleneck | Lokasi | Severity | Detail |
|------------|--------|----------|--------|
| **Tree rebuild tiap frame** | `ComponentScreen::draw()` | **HIGH** | Setiap redraw, `arena.reset()` + `build(arena)` + `layout()` + `render()` jalan ulang. Meskipun O(1) arena reset, tree yang sama dibangun ulang walau tidak ada perubahan model. |
| **Double layout pass** | `component_runtime.cpp:82-95` | **MEDIUM** | `renderComponentFrame()` calls `layout()` sekali, lalu kalau `ensureVisible()` menggeser scroll, calls `layout()` lagi (re-flow). Bisa 2 pass layout setiap frame. |
| **Per-pixel font rendering** | `canvas.cpp:117-129` | **MEDIUM** | Setiap glyph di-render pixel-by-pixel via `drawPixel()`. Untuk 5x8 font, itu 40 `drawPixel()` call per glyph. Pada layar 128x64 (~17 chars per baris × 4 baris = 68 chars), itu ~2,720 `drawPixel()` calls hanya untuk teks. |
| **Per-pixel bitmap blit** | `canvas.cpp:93-101` | **LOW** | `drawBitmap()` loops per-pixel. Untuk icon 8x8 = 64 calls. Impact kecil untuk 16 icon, tapi akan besar untuk fullscreen bitmap. |
| **Box_rounded 4 corner cuts** | `draw.cpp:47-64` | **LOW** | 4x `drawPixel(false)` per rounded box. Marginal. |
| **No dirty region tracking** | Seluruh pipeline | **HIGH** | Setiap redraw menggambar **seluruh canvas dari nol**. Tidak ada partial update. Untuk e-ink (yang bisa partial refresh), ini waste besar. |
| **Synchronous flush on slow panel** | `AsyncDisplayDriver` ESP32 path | **LOW** (mitigated) | Triple-buffered async sudah menghandle ini — flush() hanya swap buffer + signal task. Tapi kalau tidak pakai AsyncDisplayDriver, flush bisa blocking. |
| **5ms sleep di loop** | `gui_service.cpp:209` | **LOW** | `Thread::sleepMs(5)` di akhir loop. Untuk 60fps (~16ms), ini fine. Untuk >30fps, 5ms tick artinya maks ~200fps theoretical tapi tidak ada frame pacing. |
| **Rebuild tree walau idle** | `ComponentScreen::draw()` | **HIGH** | Saat screen idle (tidak ada input, tidak ada tick state change), tree tetap di-build ulang setiap redraw. Tidak ada caching / invalidation. |

### Frame Timing (dari AetherServer::renderFrame)

```
lastDrawMs_  = canvas.clear() + status bar + screen.draw() + FPS
lastFlushMs_ = canvas.flush() hingga physical done
```

Nilai ini bisa dipantau via FPS overlay (`showFps_`).

### Current FPS Cap

- **Loop rate**: ~200 fps theoretical (5ms sleep)
- **Actual render**: tergantung kompleksitas `screen.draw()`. HomeScreen carousel (3 tiles + banner + posbar + dolphin) ~2-5ms. Settings screen (8 ListItems) ~1-3ms.
- **Flush rate**: TFT ~20-30ms (SPI 40MHz, 240x320). E-ink ~500-1500ms.
- **Effective FPS**: TFT ~15-30 fps (flush-bound). E-ink ~0.5-2 fps (panel-bound).
- **AsyncDisplayDriver** mendecouple ini — UI thread bisa render 60fps walau panel lambat (latest-wins).

---

## 2. Audit Saat Ini: Screen Stack & Navigasi

### Current Implementation (ViewDispatcher, view_dispatcher.cpp:1-67)

```cpp
std::vector<IScreen*> stack_;   // simple vector

push(screen)  → stack_.push_back(&screen); screen.enter();
pop()         → stack_.pop_back();  reveal previous.enter()
popToRoot()   → stack_.resize(1);  base.enter()
active()      → stack_.back()
previous()    → stack_[size-2]      // for Modal backdrop
```

### Gaps vs Android-style Navigation

| Fitur Android | Current Aether | Gap |
|---------------|----------------|-----|
| `navigate(screen)` — push | `push(screen)` | ✅ Ada |
| `replace(screen)` — ganti current tanpa push | ❌ Tidak ada | Harus pop + push manual |
| `navigateAndClear(screen)` — ganti + clear history | ❌ Tidak ada | Harus popToRoot + push manual |
| `goBack()` — pop | `pop()` | ✅ Ada |
| `canGoBack()` — cek bisa pop | `stack_.size() > 1` | ❌ Tidak ada API |
| `backStack` — akses history | `stack_` private | ❌ Tidak ada API |
| `savedStateHandle` — state per screen | ❌ Tidak ada | Screen state hilang saat screen di-pop |
| Intent / arguments passing | ❌ Tidak ada | Tidak ada cara pass data antar screen |
| `onResume` / `onPause` lifecycle | Hanya `enter()` | ❌ Tidak ada pause/resume lifecycle |
| `onBackPressed()` interceptor | Tidak ada | Back langsung pop, tidak bisa intercept |
| Deep link navigation | ❌ Tidak ada | Tidak bisa navigate ke screen spesifik dari luar |
| `singleTop` — reuse jika sudah di atas | ❌ Tidak ada | Push selalu buat instance baru |
| Animated transitions | ❌ Tidak ada | Tidak ada slide/fade antar screen |

### Current IScreen Lifecycle

```cpp
struct IScreen {
    virtual void enter() {}                    // called on push
    virtual void onAction(Action a) {}         // input
    virtual void draw(Canvas& c) = 0;          // render
    virtual void tick(uint64_t nowMs) {}       // per-frame update
    virtual ScreenMode mode() const;           // Normal/Fullscreen/Modal
    // MISSING: onExit, onPause, onResume, onBackPressed
};
```

---

## 3. Audit Saat Ini: App Isolation

### Current AppHost (app_host.h)

```cpp
class AppHost : public IScreen, public AppContext {
    // App runs on SEPARATE thread
    // App draws into BufferDisplay (in-RAM 1-byte-per-pixel buffer)
    // AppHost::draw() blits readyBuf_ → IDisplayDriver::flushBuffer()
    // Input forwarded via MessageQueue<InputEvent> mailbox_
    // Paused: app thread parks in waitInput() — CPU near-zero
};
```

### Isolation Level Assessment

| Isolation Aspect | Status | Detail |
|------------------|--------|--------|
| **Memory isolation** | ❌ Tidak ada | App berbagi heap dengan system. Tidak ada arena/partition memory. |
| **CPU isolation** | ✅ Ada (thread) | App berjalan di thread sendiri. Bisa di-pause (Plan 22). |
| **Crash isolation** | ⚠️ Partial | App thread crash = whole system crash (no memory protection). WASM: crash handled oleh JS engine boundary. |
| **Display isolation** | ✅ Ada | App punya BufferDisplay sendiri. Tidak bisa corrupt framebuffer system. |
| **Input isolation** | ✅ Ada | Input via mailbox — system bisa intercept sebelum app (Pause key). |
| **Resource isolation** | ⚠️ Partial | Tidak ada resource quota / sandbox. App bisa exhaust heap. |
| **Lifecycle isolation** | ❌ Minim | Hanya pause/resume. Tidak ada separate backstack per-app. |

### What's Missing for "Window Isolation"

- **Per-app ViewDispatcher**: Saat ini semua screen (system + app) share satu ViewDispatcher. Screen app di-push ke stack yang sama dengan screen system. Seharusnya setiap app punya **internal navigation stack sendiri**, dan system punya **back stack of app windows**.
- **App-level lifecycle**: onCreate, onStart, onResume, onPause, onStop, onDestroy
- **App surface**: App surface tidak bisa overlap dengan system UI (status bar, notification). Harus ada z-order window manager.
- **Multi-window**: Tidak bisa dua app visible bersamaan (split screen, PiP).

---

## 4. Audit Saat Ini: Modal / Popup System

### Current Implementation

```cpp
ScreenMode::Modal  // in AetherServer::renderFrame()
  → Draw previous screen (backdrop)
  → Draw rounded box (centered, white bg)
  → Draw current screen.draw()
```

### What Exists

| Component | Status |
|-----------|--------|
| Modal screen | `ScreenMode::Modal` — screen bisa deklarasi dirinya sbg modal |
| Rounded modal box | ✅ `aether::ui::draw::box_rounded()` |
| Backdrop dimming | ❌ Tidak ada overlay gelap di atas background |
| Dialog with buttons | ⛔ Belum ada widget `Dialog` (planned di Plan 52/60, belum implementasi) |
| Popup with timeout | ❌ Tidak ada |
| Toast notification | ❌ Tidak ada |
| Bottom sheet | ❌ Tidak ada |
| Modal stack | ❌ Tidak bisa modal di atas modal |
| Back press closes modal | ⚠️ Partial — `pop()` akan pop modal, tapi tidak ada `onBackPressed` interceptor |

### Gaps

1. **Tidak ada `Dialog` widget** — perlu builder yang menghasilkan rounded box + title + body text + Row of buttons dengan callback.
2. **Tidak ada `Popup` widget** — perlu auto-dismiss setelah timeout, icon + text.
3. **Tidak ada backdrop dimming** — saat modal muncul, background harus digelapkan (invertRect checkerboard atau fill semitransparent — sulit di 1-bit, alternatif: dither pattern).
4. **Tidak ada modal stack** — `ScreenMode::Modal` hanya support satu level. Harusnya ViewDispatcher support multiple modal screens.
5. **Tidak ada toast/notification overlay** — perlu surface yang muncul di atas current screen tanpa mengubah stack.

---

## 5. Audit Saat Ini: Animation System

### What Exists

| Feature | Location | Detail |
|---------|----------|--------|
| **Marquee scroll** | `renderer.cpp`, `draw.cpp` | `draw::marquee()` — tick-driven text scroll. Via `setRenderTick(ms)`. Only works for `TextRole::Smart` nodes when focused. |
| **Scroll momentum** | `component_runtime.cpp` | `tickMomentum()` — friction decay (0.85x) after flick release. Via `ComponentState::dragScroll`. |
| **Focus ring** | `renderer.cpp` | `canvas.invertRect()` on focused Pressable. Instant toggle, no animation. |
| **Gesture timing** | `gesture.h` | Short/Long/Double/Chord/Hold with configurable ms thresholds. Not visual animation. |

### What's Missing (from Flipper reference)

| Feature | Flipper | Aether Gap |
|---------|---------|------------|
| **Frame-based sprite animation** | `IconAnimation` — timer-based, frame cycling, fps from Icon struct, callback on each tick, `view_tie_icon_animation()` | **Tidak ada sama sekali** |
| **Animated Icon nodes** | `canvas_draw_icon_animation()` — scaled rendering | Tidak ada |
| **Character animation engine** | Dolphin system: passive/active phases, bubble sequences, weighted random selection | Tidak ada |
| **One-shot animation** | `OneShotAnimationView` — play once then stop | Tidak ada |
| **Transition animations** | Tidak ada di Flipper juga, tapi perlu | Slide in/out, fade |
| **Loading spinner** | `loading_alloc()` — animated frame cycling | Tidak ada |
| **Animated progress** | `elements_progress_bar()` — bisa di-tick untuk animasi | Ada progress bar, tapi statis |
| **Easing functions** | Tidak ada di Flipper juga | Perlu untuk smooth transitions |

### Current Animation Tick Mechanism

```
GuiService::loop()
  → setRenderTick(now)     // global marquee clock
  → server_->renderFrame()
    → render() → paint() → draw::marquee(..., s_renderTick)
```

`setRenderTick` hanya digunakan untuk marquee. Tidak ada animation manager yang mengelola multiple animations.

---

## 6. Audit Saat Ini: Font System

### What Exists

```cpp
// canvas.h — single font
struct BitmapFont {
    const uint8_t* data;       // glyph data: charW bytes per glyph, column-packed
    uint8_t charW;             // 5
    uint8_t charH;             // 8
    uint8_t firstChar;         // 0x20
    uint8_t numChars;          // 95
    uint8_t spacing;           // 1
};

// font_5x8.cpp — sole font
const BitmapFont FONT_5X8;   // 95 ASCII glyphs, 5x8, 5 bytes per glyph

// text_style.h/cpp — role-based font selection
FontSpec { const BitmapFont* font; uint8_t scale; }
fontForRole(TextRole) → FontSpec
  Body    → { &FONT_5X8, theme.body_scale }     // scale 1
  Title   → { &FONT_5X8, theme.title_scale }     // scale 2
  Caption → { &FONT_5X8, theme.caption_scale }    // scale 1
  Smart   → { &FONT_5X8, theme.body_scale }       // scale 1
```

### Gaps vs Flipper Reference

| Flipper Font | Use Case | Aether Equivalent |
|-------------|----------|-------------------|
| `FontPrimary` (Helvetica bold, 8px) | Headers, titles | `TextRole::Title` + scale 2 (5x8→10x16) |
| `FontSecondary` (haxrcorp, 7px) | Body text | `TextRole::Body` + scale 1 (5x8) |
| `FontKeyboard` (profont, 7px, monospace) | Hex dumps, terminal, keyboard | **Tidak ada** — butuh monospace font |
| `FontBigNumbers` (profont 22, 15px) | Clock, counters | **Tidak ada** — FONT_5X8 scale 3 = 15x24, tapi terlalu lebar |
| `FontBatteryPercent` (5x7, 6px) | Status bar tiny | `TextRole::Caption` + scale 1 (5x8) |
| Custom font via asset packs | User themes | **Tidak ada** |

### What's Missing

| Feature | Detail |
|---------|--------|
| **Font Registry** | Untuk register multiple fonts by name/role |
| **Monospace font** | Hex viewer, logs viewer, terminal, code editor |
| **Big number font** | Clock face, stopwatch, counter displays |
| **Custom font loading** | Dari asset pack / storage — untuk theming |
| **Per-app font** | App bisa pakai font custom, bukan cuma system font |
| **Font metrics caching** | Saat ini `textWidth()` menghitung ulang tiap call — tidak cache |
| **Unicode support** | ASCII only (0x20-0x7E). Tidak ada extended charset |

---

## 7. Plan: FPS Optimization

### 7a. Dirty Region Tracking — Partial Redraw

**Problem**: Setiap redraw menggambar seluruh canvas dari nol.

**Solution**: Track "dirty rectangles" — hanya redraw region yang berubah.

```
class DirtyRegionTracker {
    void markDirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void markAllDirty();                              // full redraw
    bool isDirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h) const;
    const Rect& dirtyBounds() const;                  // bounding box of all dirty regions
    void clear();
};

// GuiService::loop():
if (vd.takeRedraw()) {
    auto dirty = dirtyTracker_.dirtyBounds();
    // Only draw within the dirty region
    c.setClip(dirty.x, dirty.y, dirty.w, dirty.h);
    server_->renderFrame(c, vd, status_);
    c.clearClip();
    dirtyTracker_.clear();
}
```

**Implementation**: 
- `ViewDispatcher::requestRedraw()` → `markAllDirty()`
- `ViewDispatcher::requestRedraw(uint16_t x, uint16_t y, uint16_t w, uint16_t h)` → `markDirty()`
- Screen dapat memanggil `requestRedraw(region)` saat hanya sebagian UI berubah

**Nilainya besar untuk e-ink**: partial refresh jauh lebih cepat (50-200ms vs 500-1500ms).

### 7b. Rebuild Skipping — Don't Rebuild When Idle

**Problem**: Tree di-build ulang setiap frame walau tidak ada perubahan.

**Solution**: Flag `dirty_` di ComponentScreen — hanya rebuild kalau model berubah.

```cpp
class ComponentScreen : public IScreen {
    void onAction(Action a) override {
        if (dispatchNav(...)) markDirty();  // only when interaction changes UI
    }
    void draw(Canvas& c) override {
        if (dirty_) {
            arena_.reset();
            build(arena_);   // rebuild tree
            dirty_ = false;
        }
        // Always re-layout + render (layout bisa berubah karena ukuran canvas)
        renderComponentFrame(root_, c, state_, ...);
    }
};
```

**Trade-off**: Layout dan render tetap jalan tiap frame (untuk scroll momentum, marquee). Tapi `build()` — yang paling mahal (alokasi node, string copy) — hanya jalan saat perlu.

### 7c. Font Rendering — Column Batch Instead of Per-Pixel

**Problem**: Setiap glyph di-render pixel-by-pixel via `drawPixel()`.

**Solution**: Batch entire column sebagai `fillRect` atau gunakan column lookup.

```cpp
// Current (per-pixel):
for (col) for (row) if (bit) drawPixel(x+col, y+row, on);

// Optimized (column-batch — untuk 1-bit, column bisa jadi 1 fillRect jika contiguous):
// Scan colBits untuk run of 1s → fillRect per run
```

Ini mengurangi call count signifikan. Untuk FONT_5X8 5 columns, rata-rata 2-3 run per column (bukan 8 pixels).

### 7d. Double-Buffer the Node Tree

**Problem**: Tree rebuild walau tidak diperlukan, karena tree dimiliki oleh arena yang di-reset.

**Solution**: Keep the tree across frames. Hanya modifikasi node yang berubah.

**Tapi ini sulit dengan NodeArena** — arena allocator tidak support free individual nodes. Pilihan:
1. **Double-buffer**: Dua arena — satu untuk frame aktif, satu untuk build berikutnya. Swap pointer.
2. **Frame caching**: Cache tree description, rebuild hanya kalau dirty.

Pilihan 2 lebih ringan — `build()` menyimpan description tree dalam `vector<NodeDesc>` kecil, dan rebuild dari cache (instant) kecuali kalau dirty.

### 7e. Frame Pacing — Target FPS Instead of Sleep

**Problem**: `Thread::sleepMs(5)` tidak presisi — bisa oversleep.

**Solution**: Target-based pacing.

```cpp
constexpr uint32_t TARGET_FRAME_MS = 16;  // ~60fps cap
uint64_t frameStart = rt_.clock().millis();
// ... render work ...
uint64_t elapsed = rt_.clock().millis() - frameStart;
if (elapsed < TARGET_FRAME_MS)
    Thread::sleepMs(TARGET_FRAME_MS - elapsed);
```

Ini memberi frame pacing yang konsisten. Untuk panel lambat (e-ink), target bisa 1000ms.

### Priority Order for FPS Optimization

1. **Rebuild skipping (7b)** — easiest, biggest impact (tidak build tree tiap frame)
2. **Frame pacing (7e)** — simplest, smooth rendering
3. **Font column batching (7c)** — measurable perf gain for text-heavy screens
4. **Dirty region tracking (7a)** — large impact for e-ink, moderate for TFT
5. **Double-buffer tree (7d)** — complex, lower priority

---

## 8. Plan: Screen Stack ala Android

### 8a. New ViewDispatcher API

```cpp
class ViewDispatcher {
public:
    // Navigation actions
    void navigate(IScreen& screen);                    // push (current: push)
    void replace(IScreen& screen);                     // ganti current, tanpa push ke backstack
    void navigateAndClear(IScreen& screen);            // replace + clear backstack
    bool goBack();                                     // pop; return false if can't
    bool canGoBack() const;                            // check backstack
    void popTo(IScreen& screen);                       // pop until this screen is top
    void clearBackStack();                             // clear all below current

    // Screen lifecycle
    // IScreen: onResume() — called when screen becomes active (from background)
    // IScreen: onPause()  — called when another screen covers this one
    // IScreen: onStop()   — called when screen is removed from stack
    // IScreen: onBackPressed() → bool — return true to consume (prevent pop)

    // Saved state
    // IScreen: saveState(Bundle&) / restoreState(Bundle&)
    // Survives configuration changes / process death

    // Arguments passing
    void navigate(IScreen& screen, Bundle args);       // pass data
    Bundle getArguments(IScreen& screen);              // retrieve passed data

    // Back stack info
    size_t backStackSize() const;
    IScreen* backStackAt(size_t index) const;

    // Deep link
    void navigateDeepLink(const char* uri);            // e.g., "app://settings/display"
};
```

### 8b. Updated IScreen Lifecycle

```cpp
struct IScreen {
    virtual ~IScreen() = default;

    // Lifecycle
    virtual void onCreate(Bundle* savedInstanceState) {}   // first creation
    virtual void onStart() {}                              // becoming visible
    virtual void onResume() {}                             // now active (top of stack)
    virtual void onPause() {}                              // losing focus
    virtual void onStop() {}                               // no longer visible
    virtual void onDestroy() {}                            // being removed

    // Back press interception
    virtual bool onBackPressed() { return false; }         // true = consumed, no pop

    // Input (existing, unchanged)
    virtual void onAction(input::Action a) {}
    virtual void onCode(input::Code c) {}
    virtual void onPointer(const input::PointerEvent& e) {}

    // Render (existing, unchanged)
    virtual void draw(Canvas& c) = 0;
    virtual void tick(uint64_t nowMs) {}

    // Saved state
    virtual void onSaveInstanceState(Bundle& outState) {}

    // Mode (existing, unchanged)
    virtual ScreenMode mode() const { return ScreenMode::Normal; }
};
```

### 8c. Bundle for Arguments / Saved State

```cpp
// Simple key-value store untuk passing data antar screen
class Bundle {
public:
    void putString(const char* key, const char* value);
    void putInt(const char* key, int32_t value);
    void putBool(const char* key, bool value);

    const char* getString(const char* key, const char* defaultVal = "") const;
    int32_t getInt(const char* key, int32_t defaultVal = 0) const;
    bool getBool(const char* key, bool defaultVal = false) const;

    bool hasKey(const char* key) const;
    size_t size() const;

private:
    std::vector<Entry> entries_;
};
```

### 8d. SingleTop / Reuse

```cpp
enum class NavigateMode {
    Standard,     // always push new instance
    SingleTop,    // reuse if already on top
    ClearTop,     // pop all above, then push
};

void navigate(IScreen& screen, Bundle args, NavigateMode mode = NavigateMode::Standard);
```

---

## 9. Plan: Window Isolation for Apps

### 9a. Per-App Internal Navigation Stack

```
System ViewDispatcher (screen stack)
  ├── HomeScreen
  ├── SettingsScreen
  ├── AppWindow("nfc_app")         ← app surface
  │     ├── AppViewDispatcher (internal stack — owned by app)
  │     │     ├── NfcMainScreen
  │     │     ├── NfcScanScreen
  │     │     └── NfcDetailScreen
  │     └── AppHost (thread + canvas + mailbox)
  ├── AppWindow("subghz_app")
  └── ...
```

**AppWindow**: IScreen wrapper yang punya internal ViewDispatcher sendiri.

```cpp
class AppWindow : public IScreen {
public:
    AppWindow(Runtime& rt, IApp& app);

    // IScreen
    ScreenMode mode() const override { return ScreenMode::Fullscreen; }
    void draw(Canvas& c) override;                // delegates to internal VD
    void onAction(Action a) override;              // forwards to internal VD
    void onBackPressed() override;                 // internal VD goBack

    // App-level lifecycle
    void pause();
    void resume();
    void close();

    ViewDispatcher& internalStack() { return internalVd_; }

private:
    ViewDispatcher internalVd_;                    // app's own backstack
    AppHost appHost_;                              // thread + canvas + mailbox
};
```

### 9b. Window Manager (z-order)

```cpp
class WindowManager {
public:
    void addWindow(AppWindow& win);                 // add to z-order
    void removeWindow(AppWindow& win);              // remove
    void bringToFront(AppWindow& win);              // make topmost
    AppWindow* focusedWindow() const;               // topmost visible

    // System surfaces always on top of app windows
    void showSystemOverlay(IScreen& overlay);       // e.g., notification, volume
    void hideSystemOverlay();

    void render(Canvas& c, ViewDispatcher& sysVd);  // draw all windows in z-order

private:
    std::vector<AppWindow*> windows_;               // z-ordered, back→front
    IScreen* systemOverlay_ = nullptr;              // always topmost
};
```

### 9c. App Lifecycle State Machine

```
CREATED → STARTED → RESUMED (active, visible, receiving input)
     ↑        ↓
     ←─── STOPPED (not visible, can be killed)
     ↑
     ←─── DESTROYED
```

`onCreate` → `onStart` → `onResume` (app active)
User navigates away → `onPause` → `onStop` (app background)
User returns → `onRestart` → `onStart` → `onResume`
App closed → `onStop` → `onDestroy`

### Implementation Notes

- AppWindow hidup di ViewDispatcher system sebagai satu screen.
- Saat app di-pause (Plan 22: long-hold Back), ViewDispatcher system popToRoot → AppWindow::onPause/onStop dipanggil.
- Saat user kembali ke app (dari HomeScreen → Continue), ViewDispatcher system push AppWindow lagi → AppWindow::onRestart/onStart/onResume dipanggil.
- AppWindow::internalVd_ mempertahankan state app (screen mana yang terakhir visible) selama app di background.

---

## 10. Plan: Modal / Popup / Toast System

### 10a. Dialog Widget

```cpp
// Tier-2 widget builder
UiNode* Dialog(NodeArena& a,
    const char* title,                              // centered, Title role
    const char* body,                               // centered, Body role, multiline
    const DialogButton* buttons,                    // array of {label, callback, userdata}
    uint8_t buttonCount,                            // up to 3 (Left/Center/Right)
    const Icon* icon = nullptr);                    // optional icon above title

struct DialogButton {
    const char* label;
    void (*onClick)(void*);
    void* userdata;
    // Position inferred from order: first=Left, second=Center, third=Right
};
```

**Tampilan**:
```
┌────────────────────────────┐
│                            │
│         [icon]             │
│         TITLE              │
│     body text line 1       │
│     body text line 2       │
│                            │
│  [Left]  [Center]  [Right] │
└────────────────────────────┘
```

### 10b. Popup Widget (Auto-dismiss)

```cpp
// Simple notification with optional timeout
UiNode* Popup(NodeArena& a,
    const Icon* icon,                              // can be nullptr
    const char* header,                            // optional header
    const char* text,                              // body text
    uint32_t timeoutMs = 0,                        // 0 = manual dismiss only
    void (*onDismiss)(void*) = nullptr,
    void* userdata = nullptr);
```

### 10c. Toast Widget (Bottom Notification)

```cpp
// Non-blocking bottom notification
UiNode* Toast(NodeArena& a,
    const char* message,                           // single line
    uint32_t durationMs = 2000);                   // auto-hide after
```

### 10d. Modal Backdrop Dimming

Karena 1-bit tidak bisa transparansi nyata, gunakan checkerboard/dither pattern:

```cpp
void drawModalBackdrop(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // 50% dither pattern: every other pixel
    for (uint16_t row = y; row < y + h; row++)
        for (uint16_t col = x + (row % 2); col < x + w; col += 2)
            c.drawPixel(col, row, true);          // darken
}
```

Ini memberi efek "dim" ~50% pada layar 1-bit.

### 10e. Integration with ViewDispatcher

```cpp
enum class OverlayType {
    Modal,      // blocks input to background, shows dimmed backdrop
    Popup,      // blocks input to background, auto-dismiss
    Toast,      // non-blocking, passes input through
};

class ViewDispatcher {
public:
    // Show overlay on top of current screen (no push to backstack)
    void showOverlay(IScreen& overlay, OverlayType type);

    // Dismiss current overlay
    void dismissOverlay();

    // Check if overlay is active
    bool hasOverlay() const;

    // Overlay-specific: dismiss after timeout
    void dismissOverlayAfter(uint32_t ms);
};
```

---

## 11. Plan: Frame-Based Animation Library

### 11a. Animation Data Model

```cpp
// Single frame of animation — wraps 1-bit bitmap data
struct AnimationFrame {
    const uint8_t* bitmap;     // raw XBM data (width * height / 8 bytes)
    uint16_t width;
    uint16_t height;
};

// Animation definition — sequence of frames + timing
struct Animation {
    const AnimationFrame* frames;    // array of frames
    uint8_t frameCount;
    uint8_t frameRate;               // frames per second (0 = static)
    bool loop;                       // repeat from start after last frame
};

// Static animation definitions (constexpr, in flash)
#define ANIM_DEF(name, ...) \
    static const AnimationFrame name##_frames[] = { __VA_ARGS__ }; \
    static const Animation name = { name##_frames, ARRAY_SIZE(name##_frames), 4, true }
```

### 11b. Animation Instance (Runtime)

```cpp
// Runtime animation player — owns a timer + state
class AnimationPlayer {
public:
    AnimationPlayer(const Animation& def);

    void start();                                  // start/resume playback
    void stop();                                   // stop and reset to frame 0
    void pause();                                  // stop but keep current frame

    bool isPlaying() const;
    bool isLastFrame() const;
    uint8_t currentFrameIndex() const;

    // Get current frame bitmap for drawing
    const uint8_t* currentFrameData() const;
    uint16_t width() const;
    uint16_t height() const;

    // Callback — called on each frame tick (for redraw triggering)
    void setFrameCallback(void (*cb)(void*), void* ctx);

    // Advance one frame manually (for one-shot control)
    void nextFrame();

private:
    const Animation& def_;
    uint8_t frame_ = 0;
    bool playing_ = false;
    FuriTimer* timer_ = nullptr;                   // or integrate with renderTick
    void (*frameCb_)(void*) = nullptr;
    void* frameCbCtx_ = nullptr;

    void onTimerTick();                            // advance frame + callback
};
```

### 11c. Integration with Renderer — Animated Icon Node

```cpp
// New node type
enum class NodeType : uint8_t {
    View, Text, Pressable, Scroll, Slider, Icon,
    AnimatedIcon,     // NEW
};

// UiNode fields for animation
struct UiNode {
    // ...
    AnimationPlayer* animPlayer;    // runtime animation instance (caller-owned)
};
```

**Renderer paint() — AnimatedIcon case**:
```cpp
if (n->type == NodeType::AnimatedIcon && n->animPlayer) {
    uint16_t ix = n->x + s.padding;
    uint16_t iy = n->y + s.padding;
    aether::ui::draw::icon(c, ix, iy,
        n->animPlayer->currentFrameData(),
        n->animPlayer->width(),
        n->animPlayer->height());
    return;
}
```

### 11d. Animation Manager (Global Tick)

```cpp
class AnimationManager {
public:
    // Register an animation — it will be ticked each frame
    void registerAnimation(AnimationPlayer& anim);
    void unregisterAnimation(AnimationPlayer& anim);

    // Tick all registered animations — called from GuiService::loop()
    // Returns true if any animation advanced (→ requestRedraw)
    bool tickAll(uint32_t nowMs);

    // Or: get the number of active animations
    size_t activeCount() const;

private:
    std::vector<AnimationPlayer*> animations_;
};
```

Ini menggantikan `setRenderTick()` yang hanya untuk marquee. `AnimationManager::tickAll()` dipanggil di GuiService loop sebelum render — kalau ada animasi yang maju, requestRedraw.

### 11e. Widget Builder — Animated Icon

```cpp
// Builder
UiNode* AnimatedIcon(NodeArena& a, AnimationPlayer& player,
                     uint8_t width = 0, uint8_t height = 0);
// width/height = 0 → use animation's native size
```

### 11f. Use Cases

| Use Case | Animation |
|----------|-----------|
| Desktop dolphin idle | Multi-frame looping animation (~30 frames, 2 fps) |
| Desktop dolphin active | Triggered one-shot animation on button press |
| Loading spinner | 4-frame looping animation (rotating dots/arrows) |
| File browser folder open/close | 2-frame toggle animation |
| Notification icon | Pulsing icon animation |
| Boot splash | Play-once logo animation |
| Menu item highlight | Subtle 2-frame blink/bounce |

### 11g. Animation Format

Animasi disimpan sebagai array `AnimationFrame` di flash (atau di-load dari asset pack). Format sama dengan icon static (XBM 1-bit). Tidak ada kompresi khusus di v1 — Heatshrink bisa ditambah nanti.

Build pipeline: PNG frames → `animate_encode.py` → `animations.h/c` dengan prefix `A_`.

---

## 12. Plan: Font Manager

### 12a. Font Registry

```cpp
// Font handle — logical name, bukan pointer mentah
using FontHandle = uint8_t;    // index into registry

struct FontDef {
    const char* name;          // "primary", "secondary", "mono", "big"
    const BitmapFont* font;    // font data
};

class FontRegistry {
public:
    static constexpr FontHandle PRIMARY   = 0;
    static constexpr FontHandle SECONDARY = 1;
    static constexpr FontHandle MONO      = 2;
    static constexpr FontHandle BIG_NUM   = 3;
    static constexpr FontHandle TINY      = 4;
    static constexpr uint8_t MAX_FONTS    = 8;

    // System fonts (always available)
    void registerBuiltin(FontHandle handle, const BitmapFont* font);

    // Custom fonts (from asset pack / app)
    FontHandle registerFont(const char* name, const BitmapFont* font);
    void unregisterFont(FontHandle handle);

    // Lookup
    const BitmapFont* get(FontHandle handle) const;
    FontHandle findByName(const char* name) const;
    bool has(FontHandle handle) const;

    // Singleton
    static FontRegistry& instance();

private:
    const BitmapFont* fonts_[MAX_FONTS] = {};
};

// Per-server font registry — each IDisplayServer can have its own fonts
// But for v1, one global registry is sufficient
```

### 12b. Updated FontSpec

```cpp
struct FontSpec {
    FontHandle handle;         // instead of const BitmapFont*
    uint8_t scale;             // 1 = native, 2 = double, etc.
};
```

Updated `fontForRole()`:
```cpp
FontSpec fontForRole(TextRole role) {
    auto& reg = FontRegistry::instance();
    switch (role) {
    case TextRole::Body:    return { FontRegistry::SECONDARY, theme().fonts.body_scale };
    case TextRole::Title:   return { FontRegistry::PRIMARY,   theme().fonts.title_scale };
    case TextRole::Caption: return { FontRegistry::TINY,      theme().fonts.caption_scale };
    case TextRole::Smart:   return { FontRegistry::SECONDARY, theme().fonts.body_scale };
    case TextRole::Mono:    return { FontRegistry::MONO,      theme().fonts.mono_scale };
    case TextRole::BigNum:  return { FontRegistry::BIG_NUM,   theme().fonts.bignum_scale };
    }
}
```

### 12c. Updated Canvas

```cpp
class Canvas {
public:
    // Set font by handle (replaces setFont(const BitmapFont&))
    void setFont(FontHandle handle);
    const BitmapFont& currentFont() const;
    // Keep setFont(BitmapFont&) for backward compat
    // ...
};
```

### 12d. Font Loading from Asset Pack

```cpp
// Font data format in asset pack: .bf (Bitmap Font) file
// Header: magic "BF01", charW(1), charH(1), firstChar(1), numChars(1), spacing(1)
// Body: charW * numChars bytes of glyph data

bool loadFontFromStorage(const char* path, FontHandle handle);
```

### 12e. Per-App Custom Font

```cpp
// App dapat register font via AppContext
class AppContext {
    // Register a font for this app's lifetime
    FontHandle registerFont(const char* name, const BitmapFont* font);
    void setDefaultFont(FontHandle handle);   // overrides fontForRole for this app
};
```

Font di-unregister saat app di-destroy.

### 12f. Built-in Fonts Needed

| Font | Name | Size | Use Case | Source |
|------|------|------|----------|--------|
| **FONT_5X8** | "secondary" | 5×8 | Body text | Already exists |
| **FONT_5X8_BOLD** | "primary" | 5×8 bold | Titles, headers | Need: thicker version of 5x8 |
| **FONT_6X8_MONO** | "mono" | 6×8 | Logs, hex, code | Need: new monospace font |
| **FONT_11X16_NUM** | "bignum" | 11×16 | Clock, counters | Need: 7-segment style or large numbers |
| **FONT_4X6_TINY** | "tiny" | 4×6 | Status bar | Need: condensed small font |

**Pendekatan**: Mulai dengan FONT_5X8 sebagai secondary + primary (scale 2), dan 1 font monospace baru (6×8). Sisanya menyusul.

---

## 13. Phase Execution Order

### Phase 1 — FPS Foundation (1-2 minggu)
- [ ] Rebuild skipping (7b): `dirty_` flag di ComponentScreen
- [ ] Frame pacing (7e): target-based tick
- [ ] Font column batching (7c): run-length encoding glyph render

**Goal**: 20-30% render time reduction, smoother frame delivery

### Phase 2 — Screen Stack (1-2 minggu)
- [ ] New IScreen lifecycle: onResume/onPause/onStop/onBackPressed
- [ ] Bundle for arguments + saved state
- [ ] ViewDispatcher: replace(), navigateAndClear(), canGoBack(), popTo()
- [ ] Migrate existing 13 screens to new lifecycle

**Goal**: Android-style navigation with proper lifecycle

### Phase 3 — App Window Isolation (1-2 minggu)
- [ ] AppWindow class: per-app internal ViewDispatcher
- [ ] WindowManager: z-ordered windows
- [ ] App lifecycle state machine (Created→Started→Resumed→etc)
- [ ] System overlay surface (for notifications/volume HUD)

**Goal**: Apps have their own navigation stack, isolated from system

### Phase 4 — Modal / Dialog / Popup (1 minggu)
- [ ] Dialog widget builder + renderer update
- [ ] Popup widget with auto-dismiss
- [ ] Toast widget
- [ ] Modal backdrop dimming (dither pattern)
- [ ] ViewDispatcher overlay support

**Goal**: Production-quality dialogs and notifications

### Phase 5 — Animation Library (1-2 minggu)
- [ ] Animation data model (AnimationFrame + Animation)
- [ ] AnimationPlayer runtime class
- [ ] AnimatedIcon node type + renderer update
- [ ] AnimationManager (global tick + registration)
- [ ] Loading spinner animation (4 frames)
- [ ] Desktop dolphin idle animation (port from Flipper)
- [ ] PNG → animation build pipeline

**Goal**: Frame-based animation for desktop, loading, icons

### Phase 6 — Font Manager (1-2 minggu)
- [ ] FontRegistry with built-in fonts
- [ ] New monospace font (6×8)
- [ ] Updated FontSpec → FontHandle
- [ ] Canvas::setFont(FontHandle)
- [ ] Asset pack font loading (.bf format)
- [ ] Per-app custom font support
- [ ] Migrate all screens to FontHandle

**Goal**: Multiple fonts, themable, per-app custom fonts

### Phase 7 — Dirty Region + Polish (1 minggu)
- [ ] Dirty region tracking
- [ ] Partial redraw support
- [ ] Integration with e-ink partial refresh
- [ ] FPS counter improvements
- [ ] Perf regression tests

**Goal**: Optimal rendering for all panel types

---

## Risks & Mitigation

| Risk | Mitigation |
|------|-----------|
| Lifecycle migration breaks 13 screens | Backward compat: old IScreen API still works, screens opt-in to new lifecycle |
| Per-app VD adds complexity | AppWindow is optional — simple apps can use ViewHolder directly (like Flipper) |
| Animation memory overhead | AnimationPlayer tiny (~40 bytes). Frame data stays in flash. |
| Font registry thread safety | FontRegistry is read-mostly — lock-free during render, mutex only on register/unregister |
| Dirty region tracking wrong → visual glitches | Always fall back to full redraw. Add debug overlay to visualize dirty rects. |

---

## Summary

| Area | Current | Target | Priority |
|------|---------|--------|----------|
| **FPS** | ~15-30 fps (flush-bound), rebuild tiap frame | ~30-60 fps (with frame pacing), rebuild only on change | PHASE 1 |
| **Screen Stack** | Simple push/pop/popToRoot | Android-style: replace, lifecycle, backstack, saved state | PHASE 2 |
| **App Isolation** | Shared VD, single backstack | Per-app VD, WindowManager, z-order, proper lifecycle | PHASE 3 |
| **Modal/Popup** | ScreenMode::Modal only | Dialog, Popup, Toast, backdrop dimming, overlay stack | PHASE 4 |
| **Animation** | Marquee + scroll momentum only | Frame-based animation library, AnimatedIcon, global tick manager | PHASE 5 |
| **Font** | Single FONT_5X8 | FontRegistry (5 fonts), per-app custom font, asset pack loading | PHASE 6 |
