# 14 — Retro UI Runtime (Screen Stack + Canvas + Input)

> Event-driven, 1-bit, Flipper Zero-style UI. Screen stack dengan callbacks enter/update/draw/tick. Input via Key enum. Render hanya saat ada perubahan. Tidak ada widget hierarchy — hanya Canvas primitives.

- Status: ☐ Not started
- Milestone: M5 (UI Runtime)
- Depends on: 12 (PluginContext), 13 (Canvas + IDisplayDriver)
- Blocks: 15 (Home Screen + Status Bar)

---

## Goal

Sesuai arsitektur dari ref (`kairo.h`):

```
Input (Key) → ViewDispatcher → IScreen::update(key) → requestRedraw()
                                                           ↓
                              IScreen::draw(Canvas&) → Canvas::flush()
                                                           ↓
                                               IDisplayDriver::flush()
                                                           ↓
                                          SimDisplay (frame JSON) / e-ink panel
```

- **Render-on-event**: canvas hanya di-flush saat `requestRedraw()` dipanggil — tidak ada fps loop.
- **Screen stack**: push screen baru di atas, pop kembali ke sebelumnya.
- **Plugin membuat IScreen** dan push via PluginContext.
- **Key routing**: tiap key event diteruskan ke screen aktif.

## Scope

### In scope

- `Key` enum: Up, Down, Left, Right, Ok, Back, Plus, Minus.
- `IScreen` interface: `enter()`, `update(Key)`, `draw(Canvas&)`, `tick(uint64_t)`.
- `ViewDispatcher`: screen stack, `requestRedraw()`, `takeRedraw()`, `handleKey(Key)`, `tick(now)`.
- Integrasi ke `Runtime::run()`: tick dispatcher + conditional flush.
- `PluginContext::pushScreen()`, `popScreen()`.
- Contoh: `SplashScreen` — tampilkan "KAIRO OS" ASCII art saat boot.

### Out of scope

- Input hardware (GPIO buttons) — plan 19 (Input System). Untuk sekarang inject via `{"cmd":"press_key","key":"ok"}` dari simulator Controls panel.
- Animasi transisi.
- Focus / tab navigation kompleks.
- Font selain FONT_5X8 (bisa ditambah nanti).

---

## Design

### Key enum

```cpp
// firmware/core/include/kairo/ui/key.h
namespace kairo {
enum class Key : uint8_t {
    None = 0,
    Up, Down, Left, Right,
    Ok, Back,
    Plus, Minus
};
const char* keyName(Key k);
}
```

### IScreen

```cpp
// firmware/core/include/kairo/ui/screen.h
namespace kairo {
class Canvas;

struct IScreen {
    virtual ~IScreen() = default;
    virtual void enter() {}                      // dipanggil saat screen jadi aktif
    virtual void update(Key key) {}              // input event → biasanya panggil requestRedraw
    virtual void draw(Canvas& canvas) = 0;       // render seluruh layar ke canvas
    virtual void tick(uint64_t nowMs) {}         // periodic (tiap ~1 menit atau custom)
};
}
```

### ViewDispatcher

```cpp
// firmware/core/include/kairo/ui/view_dispatcher.h
namespace kairo {

class ViewDispatcher {
public:
    void push(IScreen& screen);
    void pop();
    IScreen* active() const;
    bool empty() const;

    void requestRedraw();
    bool takeRedraw();           // main loop konsumsi flag ini

    void handleKey(Key key);     // dispatch ke active()->update(key)
    void tick(uint64_t nowMs);   // dispatch ke active()->tick(nowMs)

private:
    std::vector<IScreen*> stack_;
    bool redrawPending_ = false;
};
}
```

### Integrasi Runtime::run()

```cpp
void Runtime::run() {
    assert(phase_ == BootPhase::Running);
    // Initial draw
    if (viewDispatcher_ && viewDispatcher_->active()) {
        viewDispatcher_->requestRedraw();
    }
    while (!shutdownRequested_) {
        uint64_t now = clock().millis();
        serviceManager_->tickAll(now);
        if (viewDispatcher_) {
            viewDispatcher_->tick(now);
            if (viewDispatcher_->takeRedraw() && canvas_) {
                canvas_->clear();
                viewDispatcher_->active()->draw(*canvas_);
                canvas_->flush();
            }
        }
        platform_->idle();
    }
    // ...
}
```

`canvas_` dan `viewDispatcher_` adalah member baru di `Runtime`, dibuat di `initCore()` jika capability `"display"` ada.

### SplashScreen (contoh & boot screen)

```cpp
class SplashScreen : public IScreen {
    void draw(Canvas& c) override {
        c.clear();
        // ASCII art "KAIRO" — besar, centered
        const char* art[] = {
            "  _  __   _   ___ ____  ___  ",
            " | |/ /  / \\ |_ _|  _ \\/ _ \\ ",
            " | ' /  / _ \\ | || |_) | | | |",
            " | . \\ / ___ \\| ||  _ <| |_| |",
            " |_|\\_/_/   \\_|___|_| \\_\\\\___/ ",
        };
        uint16_t y = 40;
        for (auto line : art) {
            c.drawText(4, y, line);
            y += 9;
        }
        c.drawText(80, 130, "Loading...");
        c.drawRect(0, 0, c.width(), c.height());  // border frame
    }
};
```

### Simulator: inject key via command

Tambahkan command baru ke bridge:

```json
{"cmd":"press_key","key":"ok"}
{"cmd":"press_key","key":"down"}
```

Di `command_reader.cpp`, dispatch ke `rt.viewDispatcher().handleKey(...)`.

Di web Controls panel, tambahkan D-pad virtual:
```
     [ ↑ ]
[ ← ] [OK] [ → ]
     [ ↓ ]
  [BACK]  [+] [-]
```

### Runtime additions

```cpp
// runtime.h additions
class ViewDispatcher;
class Canvas;

class Runtime {
public:
    ViewDispatcher& view();
    Canvas& canvas();
    // ...
private:
    std::unique_ptr<ViewDispatcher> viewDispatcher_;
    std::unique_ptr<Canvas>         canvas_;        // backed by display driver
};
```

`canvas_` dibuat di `registerServices()` setelah display driver terdaftar. Jika tidak ada `"display"` capability, view/canvas tidak dibuat (runtime tetap bisa jalan headless).

---

## Tasks

- [ ] `Key` enum + `keyName()`.
- [ ] `IScreen` interface.
- [ ] `ViewDispatcher` (push/pop/handleKey/tick/requestRedraw/takeRedraw).
- [ ] `SplashScreen` contoh (ASCII art + border).
- [ ] `Runtime`: tambah `viewDispatcher_` + `canvas_` + accessors + integrasi di `run()`.
- [ ] `PluginContext::pushScreen()`, `popScreen()`.
- [ ] Bridge: command `press_key` → `rt.view().handleKey(key)`.
- [ ] Web Controls: tambah virtual D-pad (Up/Down/Left/Right/Ok/Back).
- [ ] `main.cpp`: push `SplashScreen` sebelum `rt.start()`.
- [ ] Core CMakeLists: tambah `src/ui/view_dispatcher.cpp`.
- [ ] Verifikasi: boot → SplashScreen tampil di web canvas, press key → log event.

## Acceptance criteria

- Boot → SplashScreen tampil di canvas dengan ASCII art.
- Klik Up/Down/Ok di D-pad web → event `KeyPress` ter-log, `screen.update(key)` dipanggil.
- `pop()` dari screen teratas → screen bawahnya tampil (stack bekerja).
- Tanpa display (`"display"` capability tidak ada) → runtime boot normal, view tidak dibuat, tidak crash.
- Render hanya saat `requestRedraw()` dipanggil — bukan setiap tick.

## Layout karakter pada 264×176 dengan font 5×8

```
Kolom: 264 / 6  = 44 karakter per baris
Baris: 176 / 9  = 19 baris (8px char + 1px gap)

Zona umum:
  y=0..8    → Status bar (1 baris: clock, wifi, battery)
  y=9..167  → Konten utama (17 baris)
  y=168..175→ Footer hints (1 baris: OK=select  BACK=cancel)
```

Gunakan konstanta ini konsisten di semua screen:

```cpp
namespace kairo::ui {
    constexpr uint16_t STATUS_Y   = 0;
    constexpr uint16_t CONTENT_Y  = 10;   // setelah status bar + separator
    constexpr uint16_t FOOTER_Y   = 168;
    constexpr uint16_t SCREEN_W   = 264;
    constexpr uint16_t SCREEN_H   = 176;
    constexpr uint16_t CHAR_W     = 6;    // 5px + 1px spacing
    constexpr uint16_t CHAR_H     = 9;    // 8px + 1px spacing
    constexpr uint16_t COLS       = SCREEN_W / CHAR_W;   // 44
    constexpr uint16_t CONTENT_ROWS = (FOOTER_Y - CONTENT_Y) / CHAR_H;  // ~17
}
```

## Risks / notes

- `ViewDispatcher` pakai pointer stack — ownership ada di caller (plugin/main). Pastikan IScreen hidup selama ada di stack.
- `requestRedraw()` dipanggil di `update()` — plugin harus ingat untuk panggil ini setelah mengubah state yang mempengaruhi tampilan.
- Untuk hardware e-ink: `canvas.flush()` = full refresh (~1s). UI harus desain untuk ini — tidak ada animasi, state change sedikit per interaksi. Sesuai retro aesthetic.
